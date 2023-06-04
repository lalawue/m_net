/*
 * Copyright (c) 2023 lalawue
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifdef EXAMPLE_MULTI_PROCESS_SVR_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include "mnet_core.h"

sem_t *g_psem = NULL;
int listen_fd = 0;

typedef struct
{
    unsigned char buf[32];
    int len;
} buf_t;

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

// MARK: - parent

static int
_parent_before_after_ac(int afd)
{
    if (afd == listen_fd)
    {
        // will not accept listen_fd
        return 0;
    }
    else
    {
        // accept except listen_fd
        return 1;
    }
}

static void
_parent_process_env(chann_t *svr, pid_t *child_pids, int child_count)
{
    printf("%d parent enter env\n", getpid());
    for (;;)
    {
        if (mnet_poll(MNET_MILLI_SECOND) <= 0)
        {
            break;
        }
        chann_msg_t *msg = NULL;
        while ((msg = mnet_result_next()))
        {
            printf("parent event -----\n");
        }
        sleep(1);
    }
    printf("parent env exit %d\n", getpid());
}

// MARK: - child

static int
_child_before_ac(int afd)
{
    return (sem_trywait(g_psem) == 0) ? 1 : 0;
}

static int
_child_after_ac(int afd)
{
    sem_post(g_psem);
    return 0;
}

static void
_child_process_env(chann_t *svr, int i)
{
    printf("%d child enter env %d\n", getpid(), i);
    int ac_count = 0;
    for (;;)
    {
        int cnt_count = mnet_poll(MNET_MILLI_SECOND);
        chann_msg_t *msg = NULL;
        while ((msg = mnet_result_next()))
        {
            if (msg->n == svr)
            {
                if (msg->event == CHANN_EVENT_ACCEPT)
                {
                    ac_count += 1;
                    printf("%d child accept new cnt fd %d, count %d, ac %d\n",
                           getpid(), mnet_chann_fd(msg->r), cnt_count, ac_count);
                    mnet_chann_set_opaque(msg->r, calloc(1, sizeof(buf_t)));
                }
                continue;
            }
            /* client event */
            if (msg->event == CHANN_EVENT_RECV)
            {
                buf_t *bt = mnet_chann_get_opaque(msg->n);
                int ret = mnet_chann_recv(msg->n, &bt->buf[bt->len], 10 - bt->len);
                if (ret + bt->len < 10)
                {
                    bt->len += ret;
                }
                else
                {
                    bt->len = 0;
                    bt->buf[10] = '\0';
                    // printf("%d child recv number %s\n", getpid(), bt->buf);
                    mnet_chann_send(msg->n, bt->buf, 10);
                }
            }
            else if (msg->event == CHANN_EVENT_DISCONNECT)
            {
                printf("%d child disconnect cnt %d\n", getpid(), cnt_count);
                _clear_chann(msg->n);
                mnet_chann_close(msg->n);
            }
        }
        usleep(i * (random() % 10));
    }
    printf("child env exit %d\n", getpid());
}

// MARK: - Main

int main(int argc, char *argv[])
{
    const char *ipaddr = argc > 1 ? argv[1] : "127.0.0.1:8090";

    chann_addr_t addr;
    if (mnet_parse_ipport(ipaddr, &addr) <= 0)
    {
        return 0;
    }

    mnet_init();

    chann_t *svr = mnet_chann_open(CHANN_TYPE_STREAM);
    mnet_chann_listen(svr, addr.ip, addr.port, 100);
    mnet_multi_accept_balancer(_parent_before_after_ac, _parent_before_after_ac);
    listen_fd = mnet_chann_fd(svr);

    printf("mnet version %d\n", mnet_version());
    printf("multiprocess svr start listen: %s\n", ipaddr);

    g_psem = sem_open("mnet.multiprocess.child", O_RDWR | O_CREAT, 0644, 1);

    int child_count = 4;
    pid_t child_pids[child_count];

    for (int i = 0; i < child_count; i++)
    {
        pid_t pid = fork();
        if (pid == -1)
        {
            perror("fork()");
            exit(1);
        }
        else if (pid > 0)
        {
            child_pids[i] = pid;
            if (i + 1 >= child_count)
            {
                _parent_process_env(svr, child_pids, child_count);
            }
        }
        else if (pid == 0)
        {
            mnet_multi_accept_balancer(_child_before_ac, _child_after_ac);
            mnet_multi_reset_event(1);
            _child_process_env(svr, i);
        }
    }

    mnet_fini();
}

#endif // EXAMPLE_MULTI_PROCESS_SVR_C