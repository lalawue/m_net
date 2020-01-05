

[![MIT licensed][1]][2]  [![Build Status][3]][4]


[1]: https://img.shields.io/badge/license-MIT-blue.svg
[2]: LICENSE

[3]: https://travis-ci.org/lalawue/m_net.svg?branch=master
[4]: https://travis-ci.org/lalawue/m_net





# About

m_net was a [single file](https://github.com/lalawue/m_net/blob/master/src/mnet_core.c)
cross platform network library, provide a simple and efficient interface for covenient use.

Also support LuaJIT with pull style API interface.

Support Linux/MacOS/FreeBSD/Windows, using epoll/kqueue/select underlying.

Please use gmake to build demo under FreeBSD.





# Features

- simple API in C++ wrapper
- with TCP/UDP support
- nonblocking & event driven interface
- using epoll/kqueue/select in Linux/MacOS/FreeBSD/Windows
- support LuaJIT with pull style API (Lua interface was deprecate)





# Server 

It's very convenience for use, an echo server example, with CPP wrapper:

```cpp
// subclass Chann to handle event
class CntChann : public Chann {
public:
   CntChann(Chann *c) : Chann(c) {}

   // implement virtual defaultEventHandler
   void defaultEventHandler(Chann *accept, chann_event_t event, int err) {

      if (event == CHANN_EVENT_RECV) {
         int ret = channRecv(m_buf, 256);
         channSend(m_buf, ret);
      }
      if (event == CHANN_EVENT_DISCONNECT) {
         delete this;           // release chann
      }
   }
   char m_buf[256];
};

int main(int argc, char *argv[]) {
   if (argc < 2) {
      cout << argv[0] << ": 'svr_ip:port'" << endl;
   } else {
      Chann echoSvr("tcp");

      if ( echoSvr.channListen(argv[1]) ) {
         cout << "svr start listen: " << argv[1] << endl;

         echoSvr.setEventHandler([](Chann *self, Chann *accept, chann_event_t event, int err) {
               if (event == CHANN_EVENT_ACCEPT) {
                  CntChann *cnt = new CntChann(accept);
                  char welcome[] = "Welcome to echoServ\n";
                  cnt->channSend((void*)welcome, sizeof(welcome));
                  delete accept;
               }
            });

         ChannDispatcher::startEventLoop();
      }
   }
   return 0;
}
```

the nested code need Closure support, with environment:

- Apple LLVM version 8.1.0 (clang-802.0.42)
- g++ (Ubuntu/Linaro 4.6.3-1ubuntu5) 4.6.3





# Client

with bare C:

```c
static void
_on_cnt_event(chann_msg_t *msg) {
   if (msg->event == CHANN_EVENT_RECV) {
      char buf[256] = {0};
      if (mnet_chann_recv(msg->n, buf, 256) > 0) {
         printf("%s", buf);;
         if ( fgets(buf, 256, stdin) ) {
            mnet_chann_send(msg->n, buf, strlen(buf));
         } else {
            mnet_chann_close(msg->n);
         }
      }
   }
   if (msg->event == CHANN_EVENT_DISCONNECT) {
      mnet_chann_close(msg->n);
   }
}

int
main(int argc, char *argv[]) {

   if (argc < 2) {
      printf("%s 'cnt_ip:port'\n", argv[0]);
   } else {
      chann_addr_t addr;
      if (mnet_parse_ipport(argv[1], &addr) > 0)  {
         mnet_init();
         chann_t *cnt = mnet_chann_open(CHANN_TYPE_STREAM);
         mnet_chann_set_cb(cnt, _on_cnt_event, NULL);

         printf("cnt try connect %s:%d...\n", addr.ip, addr.port);
         mnet_chann_connect(cnt, addr.ip, addr.port);
         while (mnet_poll(-1) > 0) {
         }
         mnet_fini();
      }
   }
   return 0;
}
```

In the other hand, the C interface with more flexible options.





# LuaJIT Wrapper

try 'make luajit' then run [luajit example](https://github.com/lalawue/m_net/blob/master/examples/chann_web_luajit.lua)

for LuaJIT's ffi.load() ignore package.cpath, first add 'build' directory into your system's library search path:

```
export DYLD_LIBRARY_PATH=$DYLD_LIBRARY_PATH:$PWD/build # in MacOS
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD/build # in Linux
```

```
luajit examples/chann_web_jit.lua '127.0.0.1:8080'
```

open your browser to visit '127.0.0.1:8080'.





# Example & Tests

in [examples](https://github.com/lalawue/m_net/tree/master/examples) and
[test](https://github.com/lalawue/m_net/tree/master/test) dir.



# Benchmark

benchmark for LuaJIT's examples/chann_web_luajit.lua '127.0.0.1:8080', under MacBook Pro (13-inch, 2017, Four Thunderbolt 3 Ports)

```
# ab -c 100 -n 5000 http://127.0.0.1:8080/empty

Concurrency Level:      100
Time taken for tests:   0.547 seconds
Complete requests:      5000
Failed requests:        0
Total transferred:      530000 bytes
HTML transferred:       65000 bytes
Requests per second:    9134.39 [#/sec] (mean)
Time per request:       10.948 [ms] (mean)
Time per request:       0.109 [ms] (mean, across all concurrent requests)
Transfer rate:          945.55 [Kbytes/sec] received
```


# Projects

- [m_tunnel](https://github.com/lalawue/m_tunnel): provide a secure
  socks5 interface tcp connection between local <-> remote side.

- [m_kcptun](https://github.com/lalawue/m_kcptun): provide a speedup TCP
  connection between local <-> remote, with [KCP](https://github.com/skywind3000/kcp) underlying

- [m_dnscnt](https://github.com/lalawue/m_dnscnt): asynchronous DNS query client/library, query
  every DNS server in list at one time.






# Thanks

Thanks NTP source code from https://github.com/edma2/ntp, author: Eugene Ma (edma2)
