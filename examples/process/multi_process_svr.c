/*
 * Copyright (c) 2023 lalawue
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifdef EXAMPLE_MULTI_PROCESS_SVR_C

#if defined(_WIN32) || defined(_WIN64)
int main(int argc, char *argv[]) {
    printf("not support windows.\n");
    return 0;
}
#else

#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include "mnet_core.h"

typedef struct
{
    unsigned char buf[32];
    int len;
} buf_t;

typedef struct
{
    pid_t pid;
    sem_t *sem;
    chann_t *svr;
    int listen_fd;
} process_t;
process_t pt_store;

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

void _worker_process_env(process_t *pt, int child_index);

// MARK: - monitor

static int
_monitor_before_after_ac(void *ac_context, int afd)
{
    process_t *pt = (process_t *)ac_context;
    if (afd == pt->listen_fd)
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
_monitor_process_env(process_t *pt,
                     pid_t *worker_pids,
                     int worker_count,
                     mnet_balancer_cb worker_before_ac,
                     mnet_balancer_cb worker_after_ac)
{
    int status;
    pt->pid = getpid();
    printf("%d monitor enter env\n", pt->pid);

    for (;;)
    {
        int fork_count = 0;

        for (int i = 0; i < worker_count; i++)
        {
            if ((worker_pids[i] == 0) || (-1 == waitpid(worker_pids[i], &status, WNOHANG)))
            {
                if (worker_pids[i] > 0) {
                    usleep(500 * 1000); // fork delay 0.5s
                }

                pid_t pid = fork();
                if (pid == -1)
                {
                    perror("fork()");
                    exit(1);
                }
                else if (pid == 0)
                {
                    mnet_multi_accept_balancer(pt, worker_before_ac, worker_after_ac);
                    mnet_multi_reset_event();
                    _worker_process_env(pt, i);
                    mnet_fini();
                    exit(0);
                }
                else if (pid > 0)
                {
                    worker_pids[i] = pid;
                    fork_count += 1;
                }
            }
        }

        if (fork_count > 0)
        {
            mnet_multi_accept_balancer(pt, _monitor_before_after_ac, _monitor_before_after_ac);
            mnet_multi_reset_event();
        }

        {
            mnet_poll(MNET_MILLI_SECOND);

            chann_msg_t *msg = NULL;
            while ((msg = mnet_result_next()))
            {
                if (msg->event == CHANN_EVENT_ACCEPT) {
                    printf("monitor accept event\n");
                }
            }
            usleep(1000);
        }
    }
    printf("monitor env exit %d\n", getpid());
}

// MARK: - worker

static int
_worker_before_ac(void *ac_context, int afd)
{
    process_t *pt = (process_t *)ac_context;
    return (sem_trywait(pt->sem) == 0) ? 1 : 0;
}

static int
_worker_after_ac(void *ac_context, int afd)
{
    process_t *pt = (process_t *)ac_context;
    sem_post(pt->sem);
    return 0;
}

void _worker_process_env(process_t *pt, int child_index)
{
    pt->pid = getpid();
    srandom(pt->pid);
    printf("%d worker enter env\n", pt->pid);
    int ac_count = 0;
    for (;;)
    {
        int cnt_count = mnet_poll(50);

        chann_msg_t *msg = NULL;
        while ((msg = mnet_result_next()))
        {
            // listen event
            if (msg->n == pt->svr)
            {
                if (msg->event == CHANN_EVENT_ACCEPT)
                {
                    ac_count += 1;
                    printf("%d worker accept new cnt fd %d, count %d, ac %d\n",
                           getpid(), mnet_chann_fd(msg->r), cnt_count, ac_count);
                    mnet_chann_set_opaque(msg->r, calloc(1, sizeof(buf_t)));
                }
                continue;
            }
            // client event
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
                    // printf("%d worker recv number %s\n", getpid(), bt->buf);
                    mnet_chann_send(msg->n, bt->buf, 10);
                }
            }
            else if (msg->event == CHANN_EVENT_DISCONNECT)
            {
                //printf("%d worker disconnect cnt %d\n", getpid(), cnt_count);
                _clear_chann(msg->n);
                mnet_chann_close(msg->n);
            }
        }

        usleep(50 + random() % 100);

        if (child_index == 0 && ac_count > 512)
        {
            break;
        }
    }
    printf("%d worker env exit\n", pt->pid);
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

    printf("mnet version %d\n", mnet_version());
    printf("multiprocess svr start listen: %s\n", ipaddr);

    mnet_init();

    process_t *pt = &pt_store;
    memset(pt, 0, sizeof(*pt));

    pt->svr = mnet_chann_open(CHANN_TYPE_STREAM);
    mnet_chann_listen(pt->svr, addr.ip, addr.port, 128);
    pt->listen_fd = mnet_chann_fd(pt->svr);

    pt->sem = sem_open("mnet.multiprocess.worker", O_RDWR | O_CREAT, 0644, 1);
    if (pt->sem == SEM_FAILED)
    {
        perror("sem_open");
        exit(errno);
    }

    int worker_count = 2;
    pid_t worker_pids[worker_count];
    memset(worker_pids, 0, sizeof(pid_t) * worker_count);

    _monitor_process_env(pt, worker_pids, worker_count, _worker_before_ac, _worker_after_ac);

    mnet_fini();
}

#endif // _WIN32 || _WIN64
#endif // EXAMPLE_MULTI_PROCESS_SVR_C