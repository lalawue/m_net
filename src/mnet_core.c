/* 
 * Copyright (c) 2015 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include "mnet_core.h"

#if MNET_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdint.h>
#pragma comment(lib, "ws2_32.lib")
#endif  // WIN

#if MNET_OS_MACOX
#include <sys/event.h>
#include <sys/time.h>
#endif  /* MACOSX */

#if MNET_OS_LINUX
#define _POSIX_SOURCE
#include <sys/epoll.h>
#endif  /* LINUX */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
/* #include <netinet/in.h> */
#include <net/if.h>
//#include <net/if_arp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#define MNET_BUF_SIZE (64*1024) /* 64kb default */

#define _MIN_OF(a, b) (((a) < (b)) ? (a) : (b))
#define _MAX_OF(a, b) (((a) > (b)) ? (a) : (b))

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

#endif  /* WIN */

enum {
   MNET_LOG_ERR = 1,
   MNET_LOG_INFO,
   MNET_LOG_VERBOSE,
};

enum {
   MNET_SET_READ,
   MNET_SET_WRITE,
   MNET_SET_ERROR,
   MNET_SET_MAX,
};

typedef struct s_rwbuf {
   int ptr, ptw;
   struct s_rwbuf *next;
   unsigned char *buf;
} rwb_t;

typedef struct s_rwbuf_head {
   rwb_t *head;
   rwb_t *tail;
   int count;
} rwb_head_t;

struct s_mchann {
   int fd;
   void *opaque;
   chann_state_t state;
   chann_type_t type;
   chann_msg_cb cb;
   struct sockaddr_in addr;
   socklen_t addr_len;
   rwb_head_t rwb_send;         /* fifo */
   struct s_mchann *prev;
   struct s_mchann *next;
   int64_t bytes_send;
   int64_t bytes_recv;
   int active_send_event;
   int buf_size;
#if (MNET_OS_MACOX | MNET_OS_LINUX)
   struct s_mchann *del_prev;   /* for delete */
   struct s_mchann *del_next;   /* for delete */
   uint32_t epoll_events;
#endif
};

#if (MNET_OS_MACOX | MNET_OS_LINUX)
#if MNET_OS_MACOX
typedef struct kevent mevent_t;
#else
typedef struct epoll_event mevent_t;
#endif
struct s_event {
   int size;
   int count;
   mevent_t *array;
};
#endif

typedef struct s_mnet {
   int init;
   int chann_count;
   chann_t *channs;
#if MNET_OS_WIN
   fd_set fdset[MNET_SET_MAX];  // select
#else
   int kq;                      // kqueue or epoll fd
   struct s_event chg;
   struct s_event evt;
   chann_t *del_channs;
#endif
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
void mnet_log_default(chann_t *n, int level, const char *log_string) {
   char slev[8] = { 'D', 'E', 'I', 'V'};
   printf("[%c]%p: %s", slev[level], n, log_string);
}

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
   b->buf = (unsigned char*)b + sizeof(*b);
   return b;
}

static rwb_t*
_rwb_create_tail(rwb_head_t *h) {
   if (h->count <= 0) {
      h->head = h->tail = _rwb_new();
      h->count++;
   }
   else if (_rwb_available(h->tail) <= 0) {
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
      if ((--h->count) <= 0) {
         h->head = h->tail = 0;
      }
   }
}

static void
_rwb_cache(rwb_head_t *h, unsigned char *buf, int buf_len) {
   int buf_ptr = 0;
   while (buf_ptr < buf_len) {
      rwb_t *b = _rwb_create_tail(h);
      int len = _MIN_OF((buf_len - buf_ptr), _rwb_available(b));
      memcpy(&b->buf[b->ptw], &buf[buf_ptr], len);
      b->ptw += len;
      buf_ptr += len;
   }
}

static unsigned char*
_rwb_drain_param(rwb_head_t *h, int *len) {
   rwb_t *b = h->head;
   *len = _rwb_buffered(b);
   return &b->buf[b->ptr];
}

static void
_rwb_drain(rwb_head_t *h, int drain_len) {
   while ((h->count>0) && (drain_len>0)) {
      rwb_t *b = h->head;
      int len = _MIN_OF(drain_len, _rwb_buffered(b));
      drain_len -= len;
      b->ptr += len;
      _rwb_destroy_head(h);
   }
}

static void
_rwb_destroy(rwb_head_t *h) {
   while (h->count > 0) {
      rwb_t *b = h->head;
      b->ptr = b->ptw = 0;
      _rwb_destroy_head(h);
   }
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
   if (n->state >= CHANN_STATE_CLOSED) {
      if (n->next) { n->next->prev = n->prev; }
      if (n->prev) { n->prev->next = n->next; }
      else { ss->channs = n->next; }
      _rwb_destroy(&n->rwb_send);
      ss->chann_count--;
      mm_log(n, MNET_LOG_VERBOSE, "chann destroy, count %d\n", ss->chann_count);
      n->state = (chann_state_t)-1;
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
   if (n->fd > 0) {
      mm_log(n, MNET_LOG_VERBOSE, "chann disconnect fd %d\n", n->fd);
      close(n->fd);
      _rwb_destroy(&n->rwb_send);
      n->fd = -1;
      n->state = CHANN_STATE_DISCONNECT;
#if (MNET_OS_MACOX | MNET_OS_LINUX)
      n->epoll_events = 0;
#endif
      return 1;
   }
   return 0;
}

static void
_chann_close_socket(mnet_t *ss, chann_t *n) {
   if (n->state > CHANN_STATE_CLOSED) {
#if (MNET_OS_MACOX | MNET_OS_LINUX)
      n->del_next = ss->del_channs;
      if (ss->del_channs) {
         ss->del_channs->del_prev = n;
      }
      ss->del_channs = n;
#endif
      n->state = CHANN_STATE_CLOSED;
      n->cb = NULL;
      n->opaque = NULL;
      mm_log(n, MNET_LOG_VERBOSE, "chann close fd %d\n", n->fd);
   }
}

static void
_chann_msg(chann_t *n, chann_event_t event, chann_t *r, int err) {
   chann_msg_t e;
   e.event = event;
   e.err = err;
   e.n = n;
   e.r = r;
   e.opaque = n->opaque;
   if ( n->cb ) {
      n->cb( &e );
   } else {
      mm_log(n, MNET_LOG_ERR, "chann fd %d no callback\n", n->fd);
   }
}

#if MNET_OS_WIN

/* select op
 */
static inline void
_select_add(mnet_t *ss, int fd, int set) {
   FD_SET(fd, &ss->fdset[set]);
}

static inline int
_select_isset(fd_set *set, int fd) {
   return FD_ISSET(fd, set);
}

static inline void
_select_zero(mnet_t *ss, int set) {
   FD_ZERO(&ss->fdset[set]);
}

static int
_select_poll(int microseconds) {
   int nfds = 0;
   chann_t *n = NULL;
   struct timeval tv;
   mnet_t *ss = _gmnet();
   fd_set *sr, *sw, *se;

   nfds = 0;

   _select_zero(ss, MNET_SET_READ);
   _select_zero(ss, MNET_SET_WRITE);
   _select_zero(ss, MNET_SET_ERROR);

   n = ss->channs;
   while ( n ) {
      switch (n->state) {
         case CHANN_STATE_LISTENING:
         case CHANN_STATE_CONNECTED:
            nfds = nfds<=n->fd ? n->fd+1 : nfds;
            _select_add(ss, n->fd, MNET_SET_READ);
            if ((_rwb_count(&n->rwb_send)>0) || n->active_send_event) {
               _select_add(ss, n->fd, MNET_SET_WRITE);
            }
            break;
         case CHANN_STATE_CONNECTING:
            nfds = nfds<=n->fd ? n->fd+1 : nfds;
            _select_add(ss, n->fd, MNET_SET_WRITE);
            _select_add(ss, n->fd, MNET_SET_ERROR);
            break;
         default:
            break;
      }
      n = n->next;
   }

   sr = &ss->fdset[MNET_SET_READ];
   sw = &ss->fdset[MNET_SET_WRITE];
   se = &ss->fdset[MNET_SET_ERROR];

   tv.tv_sec = microseconds >> MNET_ONE_SECOND_BIT;
   tv.tv_usec = microseconds & ((1<<MNET_ONE_SECOND_BIT)-1);

   if (select(nfds, sr, sw, se, microseconds >= 0 ? &tv : NULL) < 0) {
      if (errno != EINTR) {
         perror("select error !\n");
         return -1;
      }
   }

   n = ss->channs;
   while ( n ) {
      chann_t *nn = n->next;
      switch ( n->state ) {
         case CHANN_STATE_LISTENING:
            if ( _select_isset(sr, n->fd) ) {
               if (n->type == CHANN_TYPE_STREAM) {
                  chann_t *c = _chann_accept(ss, n);
                  if (c) _chann_msg(n, CHANN_EVENT_ACCEPT, c, 0);
               } else {
                  _chann_msg(n, CHANN_EVENT_RECV, NULL, 0);
               }
            }
            break;

         case CHANN_STATE_CONNECTING:
            if ( _select_isset(sw, n->fd) ) {
               int opt=0; socklen_t opt_len=sizeof(opt);
               getsockopt(n->fd, SOL_SOCKET, SO_ERROR, &opt, &opt_len);
               if (opt == 0) {
                  n->state = CHANN_STATE_CONNECTED;
                  _chann_msg(n, CHANN_EVENT_CONNECTED, NULL, 0);
               } else {
                  _chann_disconnect_socket(ss, n);
                  _chann_msg(n, CHANN_EVENT_DISCONNECT, NULL, opt);
               }
            }
            if ( _select_isset(se, n->fd) ) {
               _chann_disconnect_socket(ss, n);
               _chann_msg(n, CHANN_EVENT_DISCONNECT, NULL, 0);
            }
            break;

         case CHANN_STATE_CONNECTED:
            if ( _select_isset(se, n->fd) ) {
               _chann_disconnect_socket(ss, n);
               _chann_msg(n, CHANN_EVENT_DISCONNECT, NULL, 0);
            } else {
               if ( _select_isset(sr, n->fd) ) {
                  _chann_msg(n, CHANN_EVENT_RECV, NULL, 0);
               }
               if ( _select_isset(sw, n->fd) ) {
                  rwb_head_t *prh = &n->rwb_send;
                  if (_rwb_count(prh) > 0) {
                     int ret=0, len=0;
                     do {
                        unsigned char *buf = _rwb_drain_param(prh, &len);
                        ret = _chann_send(n, buf, len);
                        _rwb_drain(prh, ret);
                     } while (ret>=len && _rwb_count(prh)>0);
                  } else if ( n->active_send_event ) {
                     _chann_msg(n, CHANN_EVENT_SEND, NULL, 0);
                  }
               }
            }
            break;

         default:
            break;
      }
      if (n->state <= CHANN_STATE_CLOSED) {
         _chann_destroy(ss, n);
      }
      n = nn;
   }
   return ss->chann_count;
}

#define _evt_add(...)
#define _evt_del(...)

#else

/* kqueue/epoll op
 */
#if MNET_OS_MACOX

#define _KEV_CHG_ARRAY_SIZE 256
#define _KEV_EVT_ARRAY_SIZE 256

#define _KEV_FLAG_ERROR  EV_ERROR
#define _KEV_FLAG_HUP    EV_EOF
#define _KEV_EVENT_READ  EVFILT_READ
#define _KEV_EVENT_WRITE EVFILT_WRITE

#else  /* Linux */

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
#if MNET_OS_MACOX
   return kev->udata;
#else
   return kev->data.ptr;
#endif
}

static inline int
_kev_flags(mevent_t *kev, int flags) {
#if MNET_OS_MACOX
   return (kev->flags & flags);
#else
   return (kev->events & flags);
#endif
}

static inline int
_kev_events(mevent_t *kev, int events) {
#if MNET_OS_MACOX
   return (kev->filter == events);
#else
   return (kev->events & events);
#endif
}

static inline int
_kev_get_flags(mevent_t *kev) {
   if (kev) {
#if MNET_OS_MACOX
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
#if MNET_OS_MACOX
      return kev->filter;
#else
      return kev->events;
#endif
   }
   return -1;
}

static inline int
_kev_errno(mevent_t *kev) {
#if MNET_OS_MACOX
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
#if MNET_OS_MACOX
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
      close(ss->kq);
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
      mevent_t *kev = &chg->array[chg->count];
      memset(kev, 0, sizeof(mevent_t));
#if MNET_OS_MACOX
      kev->ident = n->fd;
      if (set == MNET_SET_READ) {
         kev->filter = EVFILT_READ;
      } else if (set == MNET_SET_WRITE) {
         kev->filter = EVFILT_WRITE;
      }
      kev->flags = EV_ADD | EV_EOF;
      kev->udata = (void*)n;
      chg->count += 1;
      mm_log(n, MNET_LOG_VERBOSE, "kq add chann filter %x (%d)\n", kev->filter);
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
         mm_log(n, MNET_LOG_ERR, "epoll fail to add chann filter %d, errno %d\n", set, errno);
         return 0;
      }
      n->epoll_events = events;
      mm_log(n, MNET_LOG_VERBOSE, "epoll add chann events %x\n", events);
#endif
      return 1;
   }
   return 0;
}

static int
_evt_del(chann_t *n, int set) {
   mnet_t *ss = _gmnet();
   struct s_event *chg = &ss->chg;
   if ( _evt_check_expand(chg) ) {
      mevent_t *kev = &chg->array[chg->count];
      memset(kev, 0, sizeof(mevent_t));
#if MNET_OS_MACOX
      kev->ident = n->fd;
      if (set == MNET_SET_READ) {
         kev->filter = EVFILT_READ;
      } else if (set == MNET_SET_WRITE) {
         kev->filter = EVFILT_WRITE;         
      }
      kev->flags = EV_DELETE;
      kev->udata = (void*)n;
      chg->count += 1;
      mm_log(n, MNET_LOG_VERBOSE, "kq del chann filter %x\n", kev->filter);
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
         mm_log(n, MNET_LOG_ERR, "epoll fail to add chann filter %d, errno %d\n", set, errno);
         return 0;
      }
      n->epoll_events = events;
      mm_log(n, MNET_LOG_VERBOSE, "epoll del chann events %x\n", events);
#endif
      return 1;
   }
   return 0;
}


static int
_evt_poll(int microseconds) {
   int nfd = 0;
   mnet_t *ss = _gmnet();
   struct s_event *evt = &ss->evt;
   struct s_event *chg = &ss->chg;

#if MNET_OS_MACOX
   struct timespec tsp;
   if (microseconds > 0) {
      static const int second_mask = (1<<MNET_ONE_SECOND_BIT) - 1;
      tsp.tv_sec = microseconds >> MNET_ONE_SECOND_BIT;
      tsp.tv_nsec = (uint64_t)(microseconds & second_mask) * 1000;
   }
   nfd = kevent(ss->kq, chg->array, chg->count, evt->array, evt->size, microseconds<=0 ? NULL : &tsp);
#else  /* LINUX */
   nfd = epoll_wait(ss->kq, evt->array, evt->size, microseconds);
#endif

   if (nfd<0 && errno!=EINTR) {
      mm_log(NULL, MNET_LOG_ERR, "kevent return %d, errno:%d\n", nfd, errno);
      return -1;
   } else {
      chg->count = 0;

      for (int i=0; i<nfd; i++) {
         mevent_t *kev = &evt->array[i];
         chann_t *n = (chann_t*)_kev_opaque(kev);

         mm_log(n, MNET_LOG_VERBOSE, "fd:%d,flags:%x,events:%x,state:%d (E:%x,H:%x,R:%x,W:%x)\n",
                n->fd, _kev_get_flags(kev), _kev_get_events(kev), n->state,
                _KEV_FLAG_ERROR, _KEV_FLAG_HUP, _KEV_EVENT_READ, _KEV_EVENT_WRITE);

         /* check error first */
         if ( _kev_flags(kev, (_KEV_FLAG_ERROR | _KEV_FLAG_HUP)) ) {
            int err = _kev_errno(kev);
            if (_kev_flags(kev, _KEV_FLAG_ERROR)) {
               mm_log(n, MNET_LOG_ERR, "chann got error: %d\n", err);
               _chann_disconnect_socket(ss, n);
               _chann_msg(n, CHANN_EVENT_DISCONNECT, NULL, err);
            } else {
               mm_log(n, MNET_LOG_VERBOSE, "chann got eof: %d\n", err);
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
                  mm_log(n, MNET_LOG_ERR, "chann fd:%d getsockopt %d\n", n->fd, opt);
                  _chann_disconnect_socket(ss, n);
                  _chann_msg(n, CHANN_EVENT_DISCONNECT, NULL, opt);
               }
               break;
            }

            case CHANN_STATE_CONNECTED: {
               if ( _kev_events(kev, _KEV_EVENT_READ) ) {
                  _chann_msg(n, CHANN_EVENT_RECV, NULL, 0);
               } else if ( _kev_events(kev, _KEV_EVENT_WRITE) ) {
                  rwb_head_t *prh = &n->rwb_send;
                  if (_rwb_count(prh) > 0) {
                     int ret=0, len=0;
                     do {
                        unsigned char *buf = _rwb_drain_param(prh, &len);
                        ret = _chann_send(n, buf, len);
                        _rwb_drain(prh, ret);
                     } while (ret>=len && _rwb_count(prh)>0);
                  } else if ( n->active_send_event ) {
                     _chann_msg(n, CHANN_EVENT_SEND, NULL, 0);
                  } else {
                     _evt_del(n, MNET_SET_WRITE);
                  }
               }
               break;
            }

            default:
               break;
         }
      }
   }

   chann_t *n = ss->del_channs;
   while (n) {
      chann_t *next = n->del_next;
      _chann_destroy(ss, n);
      n = next;
   }
   ss->del_channs = NULL;

   return ss->chann_count;
}
#endif /* WIN */

/* mnet api
 */
int
mnet_init() {
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
      _evt_init();
#endif
      ss->init = 1;
      return 1;
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
#if MNET_OS_WIN
      WSACleanup();
#else
      _kev_get_flags(NULL); // for compile warning
      _kev_get_events(NULL);
      _evt_fini();
#endif
      ss->init = 0;
      memset(ss, 0, sizeof(*ss));
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
         chann_t *n = ss->channs, *nn = NULL;
         while (n) {
            nn = n->next;
            mm_log(NULL, 0, "chann %p, %s:%d\n", n, _chann_addr(n), _chann_port(n));
            n = nn;
         }
         mm_log(NULL, 0, "------------------------\n");
      }
      return ss->chann_count;
   }
   return -1;
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
mnet_parse_ipport(char *ipport, chann_addr_t *addr) {
   if (ipport && addr) {
      char *p = strchr(ipport, ':');
      if (p > ipport) {
         addr->ip[p - ipport] = 0;
         strncpy(addr->ip, ipport, p - ipport);
         addr->port = atoi(p+1);
         return 1;
      }
   }
   return 0;
}

chann_t*
mnet_chann_open(chann_type_t type) {
   return _chann_create(_gmnet(), type, CHANN_STATE_CLOSED);
}

void mnet_chann_close(chann_t *n) {
   if (n) {
      mnet_t *ss = _gmnet();
      _chann_disconnect_socket(ss, n);
      _chann_close_socket(ss, n);
   }
}

int mnet_chann_state(chann_t *n) {
   return n ? n->state : -1;
}

static void
_chann_fill_addr(chann_t *n, const char *host, int port) {
   memset(&n->addr, 0, sizeof(n->addr));
   n->addr.sin_family = AF_INET;
   n->addr.sin_port = htons(port);
   n->addr.sin_addr.s_addr = 
      host==NULL ? htonl(INADDR_ANY) : inet_addr(host);
   n->addr_len = sizeof(n->addr);
}

static int
_chann_open_socket(chann_t *n, const char *host, int port, int backlog) {
   if (n->state <= CHANN_STATE_DISCONNECT) {
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
#if (MNET_OS_MACOX | MNET_OS_LINUX)
         {
            mnet_t *ss = _gmnet();
            if (n->del_next) { n->del_next->prev = n->del_prev; }
            if (n->del_prev) { n->del_prev->next = n->del_next; }
            else if (ss->del_channs == n) { ss->del_channs = n->del_next; }
            n->del_next = n->del_prev = NULL;
         }
#endif         
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
      mm_log(n, MNET_LOG_ERR, "chann fail to connect %d:%s\n", errno, strerror(errno));
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
mnet_chann_listen_ex(chann_t *n, const char *host, int port, int backlog) {
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
mnet_chann_set_cb(chann_t *n, chann_msg_cb cb, void *opaque) {
   if ( n ) {
      n->cb = cb;
      n->opaque = opaque;
   }
}

void
mnet_chann_active_event(chann_t *n, chann_event_t et, int active) {
   if ( n ) {
      if (et == CHANN_EVENT_SEND) {
         n->active_send_event = active;
         if (active) {
            _evt_add(n, MNET_SET_WRITE);
         } else {
            _evt_del(n, MNET_SET_WRITE);
         }
      }
   }
}

int
mnet_chann_recv(chann_t *n, void *buf, int len) {
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
            mnet_t *ss = _gmnet();
            mm_log(n, MNET_LOG_ERR, "chann %p fd:%d, recv errno = %d\n", n, n->fd, errno);
            _chann_disconnect_socket(ss, n);
            _chann_msg(n, CHANN_EVENT_DISCONNECT, NULL, errno);
         }
      }
      n->bytes_recv += ret;
      return ret;
   }
   return -1;
}

int
mnet_chann_send(chann_t *n, void *buf, int len) {
   if (n && buf && len>0 && n->state>=CHANN_STATE_CONNECTED) {
      int ret = len;
      rwb_head_t *prh = &n->rwb_send;

      if (_rwb_count(prh) > 0) {
         _rwb_cache(prh, (unsigned char*)buf, len);
         mm_log(n, MNET_LOG_VERBOSE, "chann %p fd:%d still cache %d(%d)!\n",
                n, n->fd, _rwb_buffered(prh->tail), _rwb_count(prh));
      } else {
         ret = _chann_send(n, buf, len);
         do {
            if (ret < 0) {
               if (errno == EWOULDBLOCK) {
                  ret = 0;
               } else {
                  mnet_t *ss = _gmnet();
                  mm_log(n, MNET_LOG_ERR, "chann %p fd:%d, send errno = %d\n", n, n->fd, errno);
                  _chann_disconnect_socket(ss, n);
                  _chann_msg(n, CHANN_EVENT_DISCONNECT, NULL, errno);
                  break;
               }
            }
            if (ret < len) {
               mm_log(n, MNET_LOG_VERBOSE, "chann %p fd:%d cache %d of %d!\n", n, n->fd, len - ret, len);
               _rwb_cache(prh, ((unsigned char*)buf) + ret, len - ret);
               ret = len;
               _evt_add(n, MNET_SET_WRITE);
            }
         } while (0);
      }
      return ret;
   }
   return -1;
}

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

int
mnet_poll(int microseconds) {
#if MNET_OS_WIN
   return _select_poll(microseconds);
#else
   return _evt_poll(microseconds);
#endif
}
