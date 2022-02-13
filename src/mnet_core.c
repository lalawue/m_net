/*
 * Copyright (c) 2015 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#if defined(_WIN32) || defined(_WIN64)
   #define MNET_OS_WIN 1
#elif defined(__APPLE__)
   #define MNET_OS_MACOX 1
#elif defined(__FreeBSD__)
   #define MNET_OS_FreeBSD 1
#else
   #define MNET_OS_LINUX 1
   #define _BSD_SOURCE
#endif

#if MNET_OS_WIN
#define _CRT_SECURE_NO_WARNINGS
#define _WIN32_WINNT _WIN32_WINNT_WIN8
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdint.h>
#endif  // WIN

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#if (MNET_OS_MACOX || MNET_OS_FreeBSD)
#if MNET_OS_FreeBSD
#include <sys/cdefs.h>
#undef __XSI_VISIBLE
#define __XSI_VISIBLE 1
#define __BSD_VISIBLE 1
#endif
#include <sys/types.h>
#include <sys/event.h>
#endif  /* MACOSX, FreeBSD */

#if MNET_OS_LINUX
#include <sys/types.h>
#include <sys/epoll.h>
#endif  /* LINUX */

#if (MNET_OS_MACOX || MNET_OS_LINUX || MNET_OS_FreeBSD)
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
//#include <net/if_arp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <sys/time.h>
#else
#include "wepoll.h"
#endif  /* MACOSX, FreeBSD, LINUX */

#include "mnet_core.h"

#if MNET_OS_WIN

#define close(a) closesocket(a)
#define getsockopt(a,b,c,d,e) getsockopt((a),(b),(c),(char*)(d),(e))
#define setsockopt(a,b,c,d,e) setsockopt((a),(b),(c),(char*)(d),(e))
#define recv(a,b,c,d) recv((SOCKET)a,(char*)b,c,d)
#define send(a,b,c,d) send((SOCKET)a,(char*)b,c,d)
#define recvfrom(a,b,c,d,e,f) recvfrom((SOCKET)a,(char*)b,c,d,e,f)
#define sendto(a,b,c,d,e,f) sendto((SOCKET)a,(char*)b,c,d,e,f)

#undef  errno
#define errno WSAGetLastError()

#undef  EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK

int _evt_del(chann_t *n, int set);

#endif  /* WIN */

enum {
   MNET_LOG_ERR = 1,
   MNET_LOG_INFO,
   MNET_LOG_VERBOSE,
};

enum {
   MNET_SET_READ = 0,
   MNET_SET_WRITE,
   MNET_SET_DEL,
};

struct sk_link {
   struct sk_link *prev, *next;
};

#define SKIPLIST_MAX_LEVEL 32   /* Should be enough for 2^32 elements */

typedef struct {
   int level;
   int count;
   struct sk_link head[SKIPLIST_MAX_LEVEL];
} skiplist_t;

typedef struct {
   int64_t key;
   void *value;
   uint8_t level;               /* for O(1) deletion */
   struct sk_link link[0];
} skipnode_t;

typedef struct s_rwbuf {
   int ptr, ptw;
   struct s_rwbuf *next;
   uint8_t *buf;
} rwb_t;

typedef struct {
   rwb_t *head;
   rwb_t *tail;
   int count;
} rwb_head_t;

typedef struct {
   int interval;                /* micro seconds */
   int64_t time;                /* micro seconds */
   void *chann;                 /* chann pointer */
} chann_tm_t;

struct s_mchann {
   int fd;                      /* socket */
   void *opaque;                /* user defined data */
   chann_state_t state;         /* chann state */
   chann_type_t type;           /* 'tcp', 'udp', 'broadcast' */
   chann_msg_filter filter; /* msg filter for extension */
   chann_msg_cb cb;             /* chann callback for callback style */
   struct sockaddr_in addr;     /* socket address */
   socklen_t addr_len;          /* socket address length */
   rwb_head_t rwb_send;         /* fifo cached for unsend data*/
   struct s_mchann *prev;       /* double linked prev chann */
   struct s_mchann *next;       /* double linked next chann */
   int64_t bytes_send;          /* bytes sended */
   int64_t bytes_recv;          /* bytes received */
   int buf_size;                /* system socket buffer size */   
   uint8_t active_send_event;   /* notify user send data buffer empty */
   skipnode_t *timer_node;      /* timer node */
   chann_msg_t msg;             /* chann message body */
   struct s_mchann *del_next;   /* for delete */
   uint32_t epoll_events;
   struct s_mchann *tmp_next;   /* for pull style */
};

#if (MNET_OS_MACOX || MNET_OS_FreeBSD)
typedef struct kevent mevent_t;
#else
typedef struct epoll_event mevent_t;
#endif

#if MNET_OS_WIN
typedef HANDLE kq_t;
#else
typedef int kq_t;
#endif

struct s_event {
   int size;
   int count;
   mevent_t *array;
};

typedef struct {
   int init;
   int chann_count;
   chann_t *channs;
   skiplist_t *tm_clock;
   int tm_count;
   int tm_capacity;
   kq_t kq;                     /* kqueue or epoll fd */
   struct s_event chg;
   struct s_event evt;
   chann_t *del_channs;
   int api_style;               /* 0:callback 1:pull style */
   poll_result_t poll_result;
   rw_result_t rw_result;
} mnet_t;

static mnet_t g_mnet;

static inline mnet_t*
_gmnet() {
   return &g_mnet;
}

/* declares
 */
static inline void mnet_log_default(chann_t *n, int level, const char *log_string);

/* basic
 */
static inline void* _mmalloc(int n) { return malloc(n); }
static inline void* _mrealloc(void*p, int n) { return realloc(p, n); }
static inline void  _mfree(void *p) { free(p); }

static void* (*mnet_malloc)(int) = _mmalloc;
static void* (*mnet_realloc)(void*, int) = _mrealloc;
static void  (*mnet_free)(void*) = _mfree;
static void  (*mnet_log)(chann_t*, int, const char*) = mnet_log_default;

static inline void* mm_malloc(int n) {
   void *p = mnet_malloc(n); memset(p, 0, n); return p;
}
static inline void* mm_realloc(void *p, size_t n) {
   return mnet_realloc(p, n);
}
static inline void mm_free(void *p) {
   mnet_free(p);
}

static int _log_level = MNET_LOG_INFO;
static inline void mm_log(chann_t *n, int level, const char *fmt, ...) {
   if (level <= _log_level) {
      char buf[192];
      va_list argptr;
      va_start(argptr, fmt);
      vsprintf(buf, fmt, argptr);
      va_end(argptr);
      mnet_log(n, level, buf);
   }
}
//#define mm_log(...)             /* use to disable any log handling */
void
mnet_log_default(chann_t *n, int level, const char *log_string) {
   static const char slev[8] = { 'D', 'E', 'I', 'V'};
   printf("[%c]%p: %s", slev[level], n, log_string);
}

static inline int
_min_of(int a, int b) {
   return a < b ? a : b;
}

static inline void
_reset_msg(poll_result_t *result, chann_t *n) {
   if (result->reserved != n) {
      memset(&n->msg, 0, sizeof(n->msg));
      n->tmp_next = NULL;
   }
}

void _chann_msg(chann_t *n, chann_event_t event, chann_t *r, int err);
int _chan_filter(chann_t *n, chann_event_t event, chann_t *r, int err);

#define _is_callback_style_api() (_gmnet()->api_style == 0)

/* buf op
 */
static inline int
_rwb_count(rwb_head_t *h) { return h->count; }

static inline int
_rwb_buffered(rwb_t *b) {
   return b ? (b->ptw - b->ptr) : 0;
}

static inline int
_rwb_available(rwb_t *b) {
   return b ? (MNET_BUF_SIZE - b->ptw) : 0;
}

static inline rwb_t*
_rwb_new(void) {
   rwb_t *b = (rwb_t*)mm_malloc(sizeof(rwb_t) + MNET_BUF_SIZE);
   b->buf = (uint8_t *)b + sizeof(rwb_t);
   return b;
}

static rwb_t*
_rwb_create_tail(rwb_head_t *h) {
   if (h->count <= 0) {
      h->head = h->tail = _rwb_new();
      h->count++;
   } else if (_rwb_available(h->tail) <= 0) {
      h->tail->next = _rwb_new();
      h->tail = h->tail->next;
      h->count++;
   }
   return h->tail;
}

static void
_rwb_destroy_head(rwb_head_t *h) {
   if (_rwb_buffered(h->head) <= 0) {
      rwb_t *b = h->head;
      h->head = b->next;
      mm_free(b);
      h->count -= 1;
      if (h->count <= 0) {
         h->head = h->tail = 0;
      }
   }
}

static void
_rwb_cache(rwb_head_t *h, uint8_t *buf, int buf_len) {
   int buf_ptr = 0;
   while (buf_ptr < buf_len) {
      rwb_t *b = _rwb_create_tail(h);
      int len = _min_of((buf_len - buf_ptr), _rwb_available(b));
      memcpy(&b->buf[b->ptw], &buf[buf_ptr], len);
      b->ptw += len;
      buf_ptr += len;
   }
}

static uint8_t*
_rwb_drain_param(rwb_head_t *h, int *len) {
   rwb_t *b = h->head;
   *len = _rwb_buffered(b);
   return &b->buf[b->ptr];
}

static void
_rwb_drain(rwb_head_t *h, int drain_len) {
   while ((h->count>0) && (drain_len>0)) {
      rwb_t *b = h->head;
      int len = _min_of(drain_len, _rwb_buffered(b));
      drain_len -= len;
      b->ptr += len;
      _rwb_destroy_head(h);
   }
}

static void
_rwb_destroy(rwb_head_t *h) {
   while (h->count > 0) {
      _rwb_destroy_head(h);
   }
}

/* skip list
 */

static inline void
list_init(struct sk_link *link) {
   link->prev = link->next = link;
}

static inline void
__list_add(struct sk_link *link, struct sk_link *prev, struct sk_link *next) {
   link->next = next;
   link->prev = prev;
   next->prev = link;
   prev->next = link;
}

static inline void
__list_del(struct sk_link *prev, struct sk_link *next) {
   prev->next = next;
   next->prev = prev;
}

static inline void
list_del(struct sk_link *link) {
   __list_del(link->prev, link->next);
   list_init(link);
}

static inline int
list_empty(struct sk_link *link) {
   return link->next == link;
}

#define list_entry(ptr, type, member)                           \
   ((type *)((char *)(ptr) - (size_t)(&((type *)0)->member)))

#define skiplist_foreach(pos, end)              \
   for (; pos != end; pos = pos->next)

#define skiplist_foreach_safe(pos, n, end)                      \
   for (n = pos->next; pos != end; pos = n, n = pos->next)

static skipnode_t*
skipnode_create(int level, int64_t key, void *value) {
   skipnode_t *node = (skipnode_t *)mm_malloc(sizeof(*node) + level * sizeof(struct sk_link));
   if (node) {
      node->key = key;
      node->value = value;
      node->level = level;
   }
   return node;
}

static inline void
skipnode_destroy(skipnode_t *node) {
   mm_free(node);
}

static skiplist_t*
skiplist_create(void) {
   skiplist_t *list = (skiplist_t *)mm_malloc(sizeof(*list));
   if (list) {
      list->level = 1;
      list->count = 0;
      size_t i = 0;
      for (i = 0; i < sizeof(list->head) / sizeof(list->head[0]); i++) {
         list_init(&list->head[i]);
      }
   }
   return list;
}

static void
skiplist_destroy(skiplist_t *list) {
   struct sk_link *n;
   struct sk_link *pos = list->head[0].next;
   skiplist_foreach_safe(pos, n, &list->head[0]) {
      skipnode_t *node = list_entry(pos, skipnode_t, link[0]);
      skipnode_destroy(node);
   }
   mm_free(list);
}

static int
skiplist_random_level(void) {
   int level = 1;
   const double p = 0.25;
   while ((rand() & 0xffff) < 0xffff * p) {
      level++;
   }
   return level > SKIPLIST_MAX_LEVEL ? SKIPLIST_MAX_LEVEL : level;
}

static skipnode_t *
skiplist_insert(skiplist_t *list, int64_t key, void *value) {
   int level = skiplist_random_level();
   if (level > list->level) {
      list->level = level;
   }

   skipnode_t *node = skipnode_create(level, key, value);
   if (node != NULL) {
      int i = list->level - 1;
      struct sk_link *pos = &list->head[i];
      struct sk_link *end = &list->head[i];

      for (; i >= 0; i--) {
         pos = pos->next;
         skiplist_foreach(pos, end) {
            skipnode_t *nd = list_entry(pos, skipnode_t, link[i]);
            if (nd->key >= key) {
               end = &nd->link[i];
               break;
            }
         }
         pos = end->prev;
         if (i < level) {
            __list_add(&node->link[i], pos, end);
         }
         pos--;
         end--;
      }

      list->count++;
   }
   return node;
}

static void
skiplist_remove_skipnode(skiplist_t *list, skipnode_t *node) {
   int i, level = node->level;
   for (i = 0; i < level; i++) {
      list_del(&node->link[i]);
      if (list_empty(&list->head[i])) {
         list->level--;
      }
   }
   skipnode_destroy(node);
   list->count--;
}

/* timer op
 */

#define _tm_count(clock) (clock->count)

static int64_t
_tm_current() {
#ifdef MNET_OS_WIN
   FILETIME ft;
   int64_t t;
   GetSystemTimeAsFileTime(&ft);
   t = (int64_t)ft.dwHighDateTime << 32 | ft.dwLowDateTime;
   return t / 10 - 11644473600000000; /* Jan 1, 1601 */
#else
   struct timeval tv;
   gettimeofday(&tv, NULL);
   return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
#endif
}

static chann_tm_t*
_tm_active(skiplist_t *clock, chann_t *n, int64_t interval) {
   chann_tm_t *it = (chann_tm_t *)mm_malloc(sizeof(*it));
   it->interval = interval;
   it->time = _tm_current() + interval;
   it->chann = n;
   n->timer_node = skiplist_insert(clock, it->time, it);
   mm_log(n, MNET_LOG_VERBOSE, "chann create timer , %p -> %zd microsecond (%d)\n", n, interval, _tm_count(clock));
   return it;
}

static inline void
_tm_update(skiplist_t *clock, chann_t *n, int64_t interval) {
   skipnode_t *snode = (skipnode_t *)n->timer_node;
   chann_tm_t *tm = (chann_tm_t *)snode->value;
   tm->interval = interval;
   tm->time = _tm_current() + tm->interval;
   skiplist_remove_skipnode(clock, snode);
   n->timer_node = skiplist_insert(clock, tm->time, tm);
   mm_log(n, MNET_LOG_VERBOSE, "chann update timer, %p -> %zd microsecond (%d)\n", n, interval, _tm_count(clock));
}

static void
_tm_kill(skiplist_t *clock, chann_t *n) {
   if (n->timer_node) {
      skipnode_t *snode = (skipnode_t *)n->timer_node;
      if (snode->value) {
         mm_free(snode->value);
      }
      skiplist_remove_skipnode(clock, snode);
      n->timer_node = NULL;
   }
}

static int
_tm_schedule(skiplist_t *clock) {
   if (clock->count <= 0) {
      return 0;
   }
   int64_t current = _tm_current();
   struct sk_link *pos = clock->head[0].next;
   skipnode_t *snode = list_entry(pos, skipnode_t, link[0]);
   chann_tm_t *tm = (chann_tm_t *)snode->value;
   if (tm->time > current) {
      return 0;
   }
   mnet_t *ss = _gmnet();
   struct sk_link *n;
   skiplist_foreach_safe(pos, n, &clock->head[0]) {
      snode = list_entry(pos, skipnode_t, link[0]);
      tm = (chann_tm_t *)snode->value;
      if (tm->time <= current) {
         chann_t *n = (chann_t *)tm->chann;
         _reset_msg(&ss->poll_result, n);
         _chann_msg(n, CHANN_EVENT_TIMER, NULL, 0);
         _tm_update(clock, n, tm->interval);
      } else {
         break;
      }
   }
   return 1;
}

/* socket param op
 */
static int
_set_nonblocking(int fd) {
#if MNET_OS_WIN
   u_long imode = 1;
   int ret = ioctlsocket(fd, FIONBIO, (u_long*)&imode);
   if (ret == NO_ERROR) return 0;
#else
   int flag = fcntl(fd, F_GETFL, 0);
   if (flag != -1) return fcntl(fd, F_SETFL, flag | O_NONBLOCK);
#endif
   return -1;
}

static int
_set_broadcast(int fd) {
   int opt = 1;
   return setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (char*)&opt, sizeof(opt));
}

static int
_set_keepalive(int fd) {
   int opt = 1;
   return setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char*)&opt, sizeof(opt));
}

static int
_set_reuseaddr(int fd) {
   int opt = 1;
   return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
}

static int
_set_bufsize(int fd, int buf_size) {
   return (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char*)&buf_size, sizeof(buf_size)) |
           setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&buf_size, sizeof(buf_size)));
}

static int
_bind(int fd, struct sockaddr_in *si) {
   return bind(fd, (struct sockaddr*)si, sizeof(*si));
}

static int
_listen(int fd, int backlog) {
   return listen(fd, backlog > 0 ? backlog : 3);
}

/* channel op
 */
static chann_t*
_chann_create(mnet_t *ss, chann_type_t type, chann_state_t state) {
   chann_t *n = (chann_t*)mm_malloc(sizeof(*n));
   n->state = state;
   n->type = type;
   n->next = ss->channs;
   n->filter = _chann_filter;
   if (ss->channs) {
      ss->channs->prev = n;
   }
   ss->channs = n;
   ss->chann_count++;
   mm_log(n, MNET_LOG_VERBOSE, "chann create, type:%d, count %d\n", type, ss->chann_count);
   return n;
}

static void
_chann_destroy(mnet_t *ss, chann_t *n) {
   if (n->state == CHANN_STATE_CLOSED) {
      if (n->next) { n->next->prev = n->prev; }
      if (n->prev) { n->prev->next = n->next; }
      else { ss->channs = n->next; }
      _rwb_destroy(&n->rwb_send);
      _tm_kill(ss->tm_clock, n);
      ss->chann_count--;
      mm_log(n, MNET_LOG_VERBOSE, "chann destroy %p (%d)\n", n, ss->chann_count);
      mm_free(n);
   }
}

static inline char*
_chann_addr(chann_t *n) {
   return inet_ntoa(n->addr.sin_addr);
}

static inline int
_chann_port(chann_t *n) {
   return ntohs(n->addr.sin_port);
}

static chann_t*
_chann_accept(mnet_t *ss, chann_t *n) {
   struct sockaddr_in addr;
   socklen_t addr_len = sizeof(addr);
   int fd = accept(n->fd, (struct sockaddr*)&addr, &addr_len);
   if (fd > 0) {
      if (_set_nonblocking(fd) >= 0) {
         chann_t *c = _chann_create(ss, n->type, CHANN_STATE_CONNECTED);
         c->fd = fd;
         c->addr = addr;
         c->addr_len = addr_len;
         mm_log(n, MNET_LOG_VERBOSE, "chann accept %p fd %d, from %s, count %d\n",
                c, c->fd, _chann_addr(c), ss->chann_count);
         return c;
      }
   }
   return NULL;
}

static int
_chann_send(chann_t *n, void *buf, int len) {
   int ret = 0;
   if (n->type == CHANN_TYPE_STREAM) {
      ret = (int)send(n->fd, buf, len, 0);
   } else {
      ret = (int)sendto(n->fd, buf, len, 0, (struct sockaddr*)&n->addr, n->addr_len);
   }
   if (ret > 0) {
      n->bytes_send += ret;
   }
   return ret;
}

static int
_chann_disconnect_socket(mnet_t *ss, chann_t *n) {
   if (n->fd > 0 && n->state > CHANN_STATE_DISCONNECT) {
      mm_log(n, MNET_LOG_VERBOSE, "chann disconnect fd:%d, %p\n", n->fd, n);
#if MNET_OS_WIN
      _evt_del(n, MNET_SET_DEL);
#endif
      close(n->fd);
      _rwb_destroy(&n->rwb_send);
      n->fd = -1;
      n->state = CHANN_STATE_DISCONNECT;
      n->epoll_events = 0;
      return 1;
   }
   return 0;
}

static void
_chann_close_socket(mnet_t *ss, chann_t *n) {
   if (n->state > CHANN_STATE_CLOSED) {
      n->del_next = ss->del_channs;
      ss->del_channs = n;
      n->state = CHANN_STATE_CLOSED;
      n->cb = NULL;
      n->opaque = NULL;
      mm_log(n, MNET_LOG_VERBOSE, "chann close %p\n", n);
   }
}

void
_chann_msg(chann_t *n, chann_event_t event, chann_t *r, int err) {
   n->msg.event = event;
   n->msg.err = err;
   n->msg.n = n;
   n->msg.r = r;
   n->msg.opaque = n->opaque;
   if (!n->filter(&n->msg)) {
      return;
   }
   if (_is_callback_style_api()) {
      if ( n->cb ) {
         n->cb( &n->msg );
      } else {
         mm_log(n, MNET_LOG_ERR, "chann fd:%d no callback\n", n->fd);
      }
   } else {
      mnet_t *ss = _gmnet();
      if (ss->poll_result.reserved != n) {
         n->tmp_next = (chann_t *)ss->poll_result.reserved;
         ss->poll_result.reserved = n;
      }
   }
}

int
_chann_filter(chann_msg_t *msg) {
   return 1;
}


static int
_chann_get_err(chann_t *n) {
   int err = 0;
   socklen_t len = sizeof(err);
   if (getsockopt(n->fd, SOL_SOCKET, SO_ERROR, (void *)&err, &len) == 0) {
      return err;
   }
   return -9999;
}

static int
_chann_try_send_rwb(chann_t *n) {
   rwb_head_t *prh = &n->rwb_send;
   if (_rwb_count(prh) > 0) {
      int ret=0, len=0;
      do {
         uint8_t *buf = _rwb_drain_param(prh, &len);
         ret = _chann_send(n, buf, len);
         if (ret > 0) {
            _rwb_drain(prh, ret);
         } else if (ret < 0 && errno != EWOULDBLOCK) {
            mnet_t *ss = _gmnet();
            mm_log(n, MNET_LOG_ERR, "chann %p fd:%d, send errno %d:%s\n",
                   n, n->fd, errno, strerror(errno));
            _chann_disconnect_socket(ss, n);
            _chann_msg(n, CHANN_EVENT_DISCONNECT, NULL, errno);
         }
      } while (ret>=len && _rwb_count(prh)>0);
      return 1;
   }
   return 0;
}

static void
_chann_fill_addr(chann_t *n, const char *host, int port) {
   memset(&n->addr, 0, sizeof(n->addr));
   n->addr.sin_family = AF_INET;
   n->addr.sin_port = htons(port);
   n->addr.sin_addr.s_addr = host==NULL ? htonl(INADDR_ANY) : inet_addr(host);
   n->addr_len = sizeof(n->addr);
}

static int
_chann_open_socket(chann_t *n, const char *host, int port, int backlog) {
   if (n->state == CHANN_STATE_DISCONNECT) {
      int istcp = n->type == CHANN_TYPE_STREAM;
      int isbc = n->type == CHANN_TYPE_BROADCAST;
      int fd = socket(AF_INET, istcp ? SOCK_STREAM : SOCK_DGRAM, 0);
      if (fd > 0) {
         int buf_size = n->buf_size>0 ? n->buf_size : MNET_BUF_SIZE;
         n->state = CHANN_STATE_DISCONNECT;

         _chann_fill_addr(n, host, port);

         if (_set_reuseaddr(fd) < 0) goto fail;
         if (backlog && _bind(fd, &n->addr) < 0) goto fail;
         if (backlog && istcp && _listen(fd,backlog) < 0) goto fail;
         if (_set_nonblocking(fd) < 0) goto fail;
         if (istcp && _set_keepalive(fd)<0) goto fail;
         if (isbc && _set_broadcast(fd)<0) goto fail;
         if (_set_bufsize(fd, buf_size) < 0) goto fail;
         mm_log(n, MNET_LOG_VERBOSE, "open socket chann:%p fd:%d\n", n, fd);
         return fd;

        fail:
         close(fd);
         perror("chann open socket: ");
      }
   }
   else if (n->type==CHANN_TYPE_DGRAM || n->type==CHANN_TYPE_BROADCAST) {
      return n->fd;
   }
   return -1;
}

/* kqueue/epoll op
 */
#if (MNET_OS_MACOX || MNET_OS_FreeBSD)

#define _KEV_CHG_ARRAY_SIZE 8
#define _KEV_EVT_ARRAY_SIZE 256

#define _KEV_FLAG_ERROR  EV_ERROR
#define _KEV_FLAG_HUP    EV_EOF
#define _KEV_EVENT_READ  EVFILT_READ
#define _KEV_EVENT_WRITE EVFILT_WRITE

#else  /* Linux, WIN */

#define _KEV_CHG_ARRAY_SIZE 4
#define _KEV_EVT_ARRAY_SIZE 256

#define _KEV_FLAG_ERROR  EPOLLERR
#define _KEV_FLAG_HUP    (EPOLLRDHUP | EPOLLHUP)
#define _KEV_EVENT_READ  EPOLLIN
#define _KEV_EVENT_WRITE EPOLLOUT

#endif

/* kev */
static inline void*
_kev_opaque(mevent_t *kev) {
#if (MNET_OS_MACOX || MNET_OS_FreeBSD)
   return kev->udata;
#else
   return kev->data.ptr;
#endif
}

static inline int
_kev_flags(mevent_t *kev, int flags) {
#if (MNET_OS_MACOX || MNET_OS_FreeBSD)
   return (kev->flags & flags);
#else
   return (kev->events & flags);
#endif
}

static inline int
_kev_events(mevent_t *kev, int events) {
#if (MNET_OS_MACOX || MNET_OS_FreeBSD)
   return (kev->filter == events);
#else
   return (kev->events & events);
#endif
}

static inline int
_kev_get_flags(mevent_t *kev) {
   if (kev) {
#if (MNET_OS_MACOX || MNET_OS_FreeBSD)
      return kev->flags;
#else
      return kev->events;
#endif
   }
   return -1;
}

static inline int
_kev_get_events(mevent_t *kev) {
   if (kev) {
#if (MNET_OS_MACOX || MNET_OS_FreeBSD)
      return kev->filter;
#else
      return kev->events;
#endif
   }
   return -1;
}

static inline int
_kev_errno(mevent_t *kev) {
#if (MNET_OS_MACOX || MNET_OS_FreeBSD)
   return kev->data;
#else
   return kev->events;
#endif   
}

/* event */
static int
_evt_init(void) {
   mnet_t *ss = _gmnet();
   if (ss->kq <= 0) {
#if (MNET_OS_MACOX || MNET_OS_FreeBSD)
      ss->kq = kqueue();
      mm_log(NULL, MNET_LOG_VERBOSE, "evt init with kqueue %d\n", ss->kq);
#else
      ss->kq = epoll_create(_KEV_EVT_ARRAY_SIZE);
      mm_log(NULL, MNET_LOG_VERBOSE, "evt init with epoll %d\n", ss->kq);
#endif
      ss->chg.size = _KEV_CHG_ARRAY_SIZE;
      ss->chg.array = (mevent_t*)mm_malloc(sizeof(mevent_t) * ss->chg.size);
      ss->evt.size = _KEV_EVT_ARRAY_SIZE;
      ss->evt.array = (mevent_t*)mm_malloc(sizeof(mevent_t) * ss->evt.size);
      return 1;
   }
   return 0;
}

static void
_evt_fini(void) {
   mnet_t *ss = _gmnet();
   if (ss->kq) {
#if MNET_OS_WIN
      epoll_close(ss->kq);
#else
      close(ss->kq);
#endif
      mm_free(ss->chg.array);
      mm_free(ss->evt.array);
      memset(&ss->chg, 0, sizeof(ss->chg));
      memset(&ss->evt, 0, sizeof(ss->evt));
      mm_log(NULL, MNET_LOG_VERBOSE, "evt fini queue %d\n", ss->kq);
      ss->kq = 0;
   }
}

static int
_evt_check_expand(struct s_event *ev) {
   if (ev->count < ev->size) {
      return 1;
   } else {
      int nsize = ev->size * 2;
      ev->array = (mevent_t*)mm_realloc(ev->array, sizeof(mevent_t) * nsize);
      if (ev->array) {
         mm_log(NULL, MNET_LOG_VERBOSE, "evt expand %p to %d\n", ev, nsize);
         ev->size = nsize;
      }
      return (ev->array != NULL);
   }
}

static int
_evt_add(chann_t *n, int set) {
   mnet_t *ss = _gmnet();
   struct s_event *chg = &ss->chg;
   if ( _evt_check_expand(chg) ) {
      mevent_t *kev = chg->array;
      memset(kev, 0, sizeof(mevent_t));
#if (MNET_OS_MACOX || MNET_OS_FreeBSD)
      kev->ident = n->fd;
      if (set == MNET_SET_READ) {
         kev->filter = EVFILT_READ;
      } else if (set == MNET_SET_WRITE) {
         kev->filter = EVFILT_WRITE;
      }
      kev->flags = EV_ADD | EV_EOF;
      kev->udata = (void*)n;
      if (kevent(ss->kq, chg->array, 1, NULL, 0, NULL) < 0) {
         mm_log(n, MNET_LOG_ERR, "kq fail to add fd:%d filter %x, errno %d:%s\n",
                n->fd, kev->filter, errno, strerror(errno));
         return 0;
      }
#else
      uint32_t events = 0;
      kev->data.ptr = (void*)n;
      if (set == MNET_SET_READ) {
         events = n->epoll_events | EPOLLIN | EPOLLRDHUP | EPOLLHUP;
      } else if (set == MNET_SET_WRITE) {
         events = n->epoll_events | EPOLLOUT | EPOLLRDHUP | EPOLLHUP;
      }
      kev->events = events;
      if (epoll_ctl(ss->kq, (n->epoll_events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD), n->fd, kev) < 0) {
         mm_log(n, MNET_LOG_ERR, "epoll fail to add fd:%d filter %x, errno %d:%s\n",
                n->fd, n->epoll_events, errno, strerror(errno));
         return 0;
      }
      n->epoll_events = events;
#endif
      mm_log(n, MNET_LOG_VERBOSE, "add chann:%p fd:%d events %d\n", n, n->fd, set);
      return 1;
   }
   return 0;
}

int
_evt_del(chann_t *n, int set) {
   mnet_t *ss = _gmnet();
   struct s_event *chg = &ss->chg;
   if ( _evt_check_expand(chg) ) {
      mevent_t *kev = chg->array;
      memset(kev, 0, sizeof(mevent_t));
#if (MNET_OS_MACOX || MNET_OS_FreeBSD)
      kev->ident = n->fd;
      if (set == MNET_SET_READ) {
         kev->filter = EVFILT_READ;
      } else if (set == MNET_SET_WRITE) {
         kev->filter = EVFILT_WRITE;         
      }
      kev->flags = EV_DELETE;
      kev->udata = (void*)n;
      if (kevent(ss->kq, chg->array, 1, NULL, 0, NULL) < 0) {
         mm_log(n, MNET_LOG_ERR, "kq fail to delete fd:%d filter %x, errno %d:%s\n",
                n->fd, kev->filter, errno, strerror(errno));
         return 0;
      }
#else
      uint32_t events = 0;
      kev->data.ptr = (void*)n;
      if (set == MNET_SET_READ) {
         events = n->epoll_events & ~EPOLLIN;
      } else if (set == MNET_SET_WRITE) {
         events = n->epoll_events & ~EPOLLOUT;
      }
      kev->events = events;
      if (epoll_ctl(ss->kq, events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL, n->fd, kev) < 0) {
         mm_log(n, MNET_LOG_ERR, "epoll fail to add fd:%d filter %x, errno %d, %s\n",
                n->fd, n->epoll_events, errno, strerror(errno));
         return 0;
      }
      n->epoll_events = events;
#endif
      mm_log(n, MNET_LOG_VERBOSE, "del chann:%p fd:%d events %d\n", n, n->fd, set);
      return 1;
   }
   return 0;
}

static void
_evt_del_channs(mnet_t *ss) {
   chann_t *n = ss->del_channs;
   while (n) {
      chann_t *next = n->del_next;
      _chann_destroy(ss, n);
      n = next;
   }
   ss->del_channs = NULL;
}

static poll_result_t*
_evt_poll(uint32_t milliseconds) {
   int nfd = 0;
   mnet_t *ss = _gmnet();
   struct s_event *evt = &ss->evt;
   struct s_event *chg = &ss->chg;

   _evt_del_channs(ss);

   memset(&ss->poll_result, 0, sizeof(ss->poll_result));   

   /* timer has highest prority */
   if (_tm_schedule(ss->tm_clock) && !_is_callback_style_api()) {
      ss->poll_result.chann_count = ss->chann_count;
      return &ss->poll_result;
   }
   
   /* kqueue/epoll read/write/error event */
#if (MNET_OS_MACOX || MNET_OS_FreeBSD)
   struct timespec tsp;
   tsp.tv_sec = milliseconds / MNET_MILLI_SECOND;
   tsp.tv_nsec = (uint64_t)(milliseconds - tsp.tv_sec * MNET_MILLI_SECOND) * 1000000;
   nfd = kevent(ss->kq, NULL, 0, evt->array, evt->size, &tsp);
#else  /* LINUX */
   nfd = epoll_wait(ss->kq, evt->array, evt->size, milliseconds);
#endif
   
   if (nfd<0 && errno!=EINTR) {
      mm_log(NULL, MNET_LOG_ERR, "kevent return %d, errno %d:%s\n", nfd, errno, strerror(errno));
      ss->poll_result.chann_count = -1;
      return &ss->poll_result;
   } else if (nfd > 0) {
      chg->count = 0;

      int i = 0;
      for (i=0; i<nfd; i++) {
         mevent_t *kev = &evt->array[i];
         chann_t *n = (chann_t*)_kev_opaque(kev);

         if (n->state == CHANN_STATE_CLOSED) {
            continue;
         }

         _reset_msg(&ss->poll_result, n);

         mm_log(n, MNET_LOG_VERBOSE, "fd:%d,flags:%x,events:%x,state:%d (E:%x,H:%x,R:%x,W:%x)\n",
                n->fd, _kev_get_flags(kev), _kev_get_events(kev), n->state,
                _KEV_FLAG_ERROR, _KEV_FLAG_HUP, _KEV_EVENT_READ, _KEV_EVENT_WRITE);

         /* check error first */
         if ( _kev_flags(kev, (_KEV_FLAG_ERROR | _KEV_FLAG_HUP)) ) {
            if (_kev_flags(kev, _KEV_FLAG_ERROR)) {
               int err = _chann_get_err(n);
               mm_log(n, MNET_LOG_ERR, "chann got error: %d:%s\n", err, strerror(err));
               _chann_disconnect_socket(ss, n);
               _chann_msg(n, CHANN_EVENT_DISCONNECT, NULL, err);
            } else {
               int err = _kev_errno(kev);
               mm_log(n, MNET_LOG_VERBOSE, "chann got eof: %d:%s\n", err, strerror(err));
               _chann_disconnect_socket(ss, n);
               _chann_msg(n, CHANN_EVENT_DISCONNECT, NULL, err);
            }
            continue;           /* error state handle down, continue to next */
         }

         switch ( n->state ) {
            case CHANN_STATE_LISTENING: {
               if (n->type == CHANN_TYPE_STREAM) {
                  chann_t *c = _chann_accept(ss, n);
                  if (c) {
                     _chann_msg(n, CHANN_EVENT_ACCEPT, c, 0);
                     _evt_add(c, MNET_SET_READ);
                  }
               } else {
                  _chann_msg(n, CHANN_EVENT_RECV, NULL, 0);
               }
               break;
            }
               
            case CHANN_STATE_CONNECTING: {
               int opt=0; socklen_t opt_len=sizeof(opt);
               getsockopt(n->fd, SOL_SOCKET, SO_ERROR, &opt, &opt_len);
               if (opt == 0) {
                  _evt_del(n, MNET_SET_WRITE);
                  _evt_add(n, MNET_SET_READ);
                  n->state = CHANN_STATE_CONNECTED;
                  _chann_msg(n, CHANN_EVENT_CONNECTED, NULL, 0);
               } else {
                  mm_log(n, MNET_LOG_ERR, "chann fd:%d getsockopt %d:%s\n", n->fd, opt, strerror(opt));
                  _chann_disconnect_socket(ss, n);
                  _chann_msg(n, CHANN_EVENT_DISCONNECT, NULL, opt);
               }
               break;
            }

            case CHANN_STATE_CONNECTED: {
               if ( _kev_events(kev, _KEV_EVENT_READ) ) {
                  _chann_msg(n, CHANN_EVENT_RECV, NULL, 0);
               } else if ( _kev_events(kev, _KEV_EVENT_WRITE) ) {
                  if (!_chann_try_send_rwb(n)) {
                     if (n->active_send_event) {
                        _chann_msg(n, CHANN_EVENT_SEND, NULL, 0);
                     } else {
                        _evt_del(n, MNET_SET_WRITE);
                     }
                  }
               }
               break;
            }

            default:
               break;
         }
      }
   }

   ss->poll_result.chann_count = ss->chann_count;
   return &ss->poll_result;
}

/* mnet api
 */
int
mnet_init(int api_style) {
   mnet_t *ss = _gmnet();
   if ( !ss->init ) {
#if MNET_OS_WIN
      WSADATA wdata;
      if (WSAStartup(MAKEWORD(2,2), &wdata) != 0) {
         mm_log(NULL, MNET_LOG_ERR, "fail to init !\n");
         return 0;
      }
      mm_log(NULL, MNET_LOG_VERBOSE, "init with select\n");
#else
      signal(SIGPIPE, SIG_IGN);
#endif
      _evt_init();      
      srand(_tm_current());
      ss->tm_clock = skiplist_create();
      ss->api_style = api_style;
      ss->init = 1;
      return 20200522;
   }
   return 0;
}

void
mnet_fini() {
   mnet_t *ss = _gmnet();
   if ( ss->init ) {
      chann_t *n = ss->channs;
      while ( n ) {
         chann_t *next = n->next;
         _chann_disconnect_socket(ss, n);
         _chann_close_socket(ss, n);
         _chann_destroy(ss, n);
         n = next;
      }
      _kev_get_flags(NULL); // for compile warning
      _kev_get_events(NULL);
      _evt_fini();
      skiplist_destroy(ss->tm_clock);
      ss->init = 0;
      memset(ss, 0, sizeof(*ss));
#if MNET_OS_WIN
      WSACleanup();
#endif
      mm_log(NULL, MNET_LOG_VERBOSE, "fini\n");      
   }
}

void
mnet_allocator(void* (*new_malloc)(int),
               void* (*new_realloc)(void*, int),
               void  (*new_free)(void*))
{
   if (new_malloc)  { mnet_malloc  = new_malloc; }
   if (new_realloc) { mnet_realloc = new_realloc; }   
   if (new_free)    { mnet_free    = new_free; }
}

void
mnet_setlog(int level, mnet_log_cb cb) {
   if (level >= 0) {
      _log_level = level;
   }
   if (cb) {
      mnet_log = cb;
   }
}

int
mnet_report(int level) {
   mnet_t *ss = _gmnet();
   if (ss->init) {
      if (level > 0) {
         mm_log(NULL, 0, "-------- channs(%d) --------\n", ss->chann_count);
         chann_t *n = ss->channs;
         while (n) {
            mm_log(NULL, 0, "chann:%p fd:%d state:%d %s:%d\n", n, n->fd, n->state, _chann_addr(n), _chann_port(n));
            n = n->next;
         }
         mm_log(NULL, 0, "------------------------\n");
      }
      return ss->chann_count;
   }
   return -1;
}

int64_t
mnet_current() {
   return _tm_current();
}

/* sync funciton will block thread */
int
mnet_resolve(char *host, int port, chann_type_t ctype, chann_addr_t *addr) {
   int ret = 0;
   if (host && addr) {
      struct sockaddr_in *valid_in = NULL;
      struct addrinfo hints, *ai=NULL, *curr=NULL;
      char buf[32] = {0};

      memset(&hints, 0, sizeof(hints));
      hints.ai_family = AF_INET;
      hints.ai_socktype = (ctype == CHANN_TYPE_DGRAM) ? SOCK_DGRAM : SOCK_STREAM;
      if (port > 0) { sprintf(buf, "%d", port); }
      if ( getaddrinfo(host, (port>0 ? buf : NULL), &hints, &ai) ) {
         mm_log(NULL, 0, "fail to resolve host !\n");
         goto fail;
      }

      for ( curr=ai; curr!=NULL; curr=curr->ai_next) {
         char ipstr[16] = {0};
         inet_ntop(AF_INET, &(((struct sockaddr_in *)(curr->ai_addr))->sin_addr), ipstr, 16);
         if (strlen(ipstr) > 7) { // '0.0.0.0'
            valid_in = (struct sockaddr_in*)curr->ai_addr;
            break;
         }
      }

      if (valid_in) {
         if (0 == getnameinfo((struct sockaddr*)valid_in, sizeof(*valid_in),
                              (char*)addr->ip, 16,NULL, 0, NI_NUMERICHOST))
         {
            ret = 1;
            addr->port = port;
         }
      }

     fail:
      freeaddrinfo(ai);
   }
   return ret;
}

int
mnet_parse_ipport(const char *ipport, chann_addr_t *addr) {
   if (ipport && addr) {
      const char *p = strchr(ipport, ':');
      if (p >= ipport) {
         if ((p - ipport) >= 7) {
            addr->ip[p - ipport] = 0;
            strncpy(addr->ip, ipport, p - ipport);
         } else {
            strncpy(addr->ip, "0.0.0.0", 8);
         }
         addr->port = atoi(p+1);
         return 1;
      }
   }
   return 0;
}

/** Channs
 */

chann_t*
mnet_chann_open(chann_type_t type) {
   return _chann_create(_gmnet(), type, CHANN_STATE_DISCONNECT);
}

void
mnet_chann_close(chann_t *n) {
   if (n && n->state > CHANN_STATE_CLOSED) {
      mnet_t *ss = _gmnet();
      _chann_disconnect_socket(ss, n);
      _chann_close_socket(ss, n);
   }
}

int
mnet_chann_fd(chann_t *n) {
   if (n) {
       return n->fd;
   }
   return -1;
}

chann_type_t
mnet_chann_type(chann_t *n) {
   if (n) {
      return n->type;
   }
   return (chann_type_t)0;
}

int
mnet_chann_state(chann_t *n) {
   return n ? n->state : -1;
}

int
mnet_chann_connect(chann_t *n, const char *host, int port) {
   if (n && host && port>0) {
      int fd = _chann_open_socket(n, host, port, 0);
      if (fd > 0) {
         n->fd = fd;
         if (n->type == CHANN_TYPE_STREAM) {
            int r = connect(fd, (struct sockaddr*)&n->addr, n->addr_len);
            if (r >= 0 || errno==EINPROGRESS || errno==EWOULDBLOCK) {
               n->state = CHANN_STATE_CONNECTING;
               _evt_add(n, MNET_SET_WRITE);
               mm_log(n, MNET_LOG_VERBOSE, "chann fd:%d type:%d connecting...\n", fd, n->type);
               return 1;
            }
         } else {
            n->state = CHANN_STATE_CONNECTED;
            _evt_add(n, MNET_SET_READ);
            mm_log(n, MNET_LOG_VERBOSE, "chann fd:%d type:%d connected\n", fd, n->type);
            return 1;
         }
      }
      mm_log(n, MNET_LOG_ERR, "chann %p fail to connect %d:%s\n", n, errno, strerror(errno));
   }
   return 0;
}

void
mnet_chann_disconnect(chann_t *n) {
   if (n) {
      _chann_disconnect_socket(_gmnet(), n);
   }
}

int
mnet_chann_listen(chann_t *n, const char *host, int port, int backlog) {
   if (n && port>0) {
      int fd = _chann_open_socket(n, host, port, backlog | 1);
      if (fd > 0) {
         n->fd = fd;
         n->state = CHANN_STATE_LISTENING;
         _evt_add(n, MNET_SET_READ);
         mm_log(n, MNET_LOG_VERBOSE, "chann %p, fd:%d listen\n", n, fd);
         return 1;
      }
      mm_log(n, MNET_LOG_ERR, "chann %p fail to listen\n", n);
   }
   return 0;
}

/* mnet channel api
 */

void
mnet_chann_set_filter(chann_t *n, chann_msg_filter filter) {
   if (n && filter) {
      n->filter = filter;
   }
}

void
mnet_chann_set_cb(chann_t *n, chann_msg_cb cb) {
   if ( n ) {
      n->cb = cb;
   }
}

void
mnet_chann_set_opaque(chann_t *n, void *opaque) {
   if (n) {
      n->opaque = opaque;
   }
}

void*
mnet_chann_get_opaque(chann_t *n) {
   return n ? n->opaque : NULL;
}

void
mnet_chann_active_event(chann_t *n, chann_event_t et, int64_t value) {
   if (n == NULL) {
      return;
   }
   if (et == CHANN_EVENT_SEND &&
       n->state == CHANN_STATE_CONNECTED &&
       n->active_send_event != !!value)
   {
      n->active_send_event = !!value;
      if (n->active_send_event) {
         _evt_add(n, MNET_SET_WRITE);
      } else if (mnet_chann_cached(n) <= 0) {
         _evt_del(n, MNET_SET_WRITE);
      }
   } else if (et == CHANN_EVENT_TIMER && n->state != CHANN_STATE_CLOSED) {
      skiplist_t *clock = _gmnet()->tm_clock;
      if (value > 0) {
         if (n->timer_node) {
            _tm_update(clock, n, value * 1000);
         } else{
            _tm_active(clock, n, value * 1000);
         }
      } else if (n->timer_node) {
         _tm_kill(clock, n);
      }
   }
}

rw_result_t*
mnet_chann_recv(chann_t *n, void *buf, int len) {
   mnet_t *ss = _gmnet();         
   if (n && buf && len>0 && n->state>=CHANN_STATE_CONNECTED) {
      int ret = 0;
      if (n->type == CHANN_TYPE_STREAM) {
         ret = (int)recv(n->fd, buf, len, 0);
      } else {
         n->addr_len = sizeof(n->addr);
         ret = (int)recvfrom(n->fd, buf, len, 0, (struct sockaddr*)&(n->addr), &(n->addr_len));
      }
      if (ret < 0) {
         if (errno == EWOULDBLOCK) {
            ret = 0;
         } else {
            mm_log(n, MNET_LOG_ERR, "chann %p fd:%d, recv errno %d:%s\n",
                   n, n->fd, errno, strerror(errno));
            _chann_disconnect_socket(ss, n);
            _chann_msg(n, CHANN_EVENT_DISCONNECT, NULL, errno);
         }
      }
      n->bytes_recv += ret;
      ss->rw_result.ret = ret;
      ss->rw_result.msg = &n->msg;
   } else {
      ss->rw_result.ret = -1;
      ss->rw_result.msg = NULL;
   }
   return &ss->rw_result;
}

rw_result_t*
mnet_chann_send(chann_t *n, void *buf, int len) {
   mnet_t *ss = _gmnet();   
   if (n && buf && len>0 && n->state>=CHANN_STATE_CONNECTED) {
      int ret = len;
      rwb_head_t *prh = &n->rwb_send;

      if (_rwb_count(prh) > 0) {
         _rwb_cache(prh, (uint8_t *)buf, len);
         mm_log(n, MNET_LOG_VERBOSE, "chann %p fd:%d still cache %d(%d)!\n",
                n, n->fd, _rwb_buffered(prh->tail), _rwb_count(prh));
      } else {
         ret = _chann_send(n, buf, len);
         if (ret < 0) {
            if (errno == EWOULDBLOCK) {
               ret = 0;
            } else {
               mm_log(n, MNET_LOG_ERR, "chann %p fd:%d, send errno %d:%s\n",
                      n, n->fd, errno, strerror(errno));
               _chann_disconnect_socket(ss, n);
               _chann_msg(n, CHANN_EVENT_DISCONNECT, NULL, errno);
            }
         }
         if (ret >= 0 && ret < len) {
            mm_log(n, MNET_LOG_VERBOSE, "chann %p fd:%d cache %d of %d!\n", n, n->fd, len - ret, len);
            _rwb_cache(prh, ((uint8_t *)buf) + ret, len - ret);
            ret = len;
            _evt_add(n, MNET_SET_WRITE);
         }
      }
      ss->rw_result.ret = ret;
      ss->rw_result.msg = &n->msg;
   } else {
      ss->rw_result.ret = -1;
      ss->rw_result.msg = NULL;
   }
   return &ss->rw_result;
}


/* set bufsize before connect or listen */
int
mnet_chann_set_bufsize(chann_t *n, int bufsize) {
   if (n && bufsize>0) {
      n->buf_size = bufsize;
      if (n->fd > 0) {
         return _set_bufsize(n->fd, bufsize);
      }
      return 1;
   }
   return 0;
}

int
mnet_chann_cached(chann_t *n) {
   if (n) {
      rwb_head_t *prh = &n->rwb_send;
      int count = _rwb_count(prh);
      if (count > 0) {
         return _rwb_buffered(prh->head) + (count - 1) * MNET_BUF_SIZE;
      }
   } 
   return 0;
}

int
mnet_chann_addr(chann_t *n, chann_addr_t *addr) {
   if (n && addr) {
      strncpy(addr->ip, _chann_addr(n), 16);
      addr->port = _chann_port(n);
      return 1;
   }
   return 0;
}

long long
mnet_chann_bytes(chann_t *n, int be_send) {
   if ( n ) {
      return (be_send ? n->bytes_send : n->bytes_recv);
   }
   return -1;
}

chann_msg_t*
mnet_result_next(poll_result_t *result) {
   if (result) {
      while (result->reserved) {
         chann_t *n = (chann_t *)result->reserved;
         result->reserved = n->tmp_next;
         return &n->msg;
      }
   }
   return NULL;
}

poll_result_t*
mnet_poll(uint32_t milliseconds) {
   return _evt_poll(milliseconds);
}

#undef _is_callback_style_api
