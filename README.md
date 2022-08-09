
[![MIT licensed][1]][2]  [![Build Status][3]][4]


[1]: https://img.shields.io/badge/license-MIT-blue.svg
[2]: LICENSE

[3]: https://travis-ci.org/lalawue/m_net.svg?branch=master
[4]: https://travis-ci.org/lalawue/m_net

- [About](#about)
- [Features](#features)
- [Server](#server)
- [Client](#client)
- [Lua/LuaJIT Wrapper](#lualuajit-wrapper)
- [DNS query](#dns-query)
- [OpenSSL support](#openssl-support)
  - [Example](#example)
  - [LuaJIT TLS wrapper](#luajit-tls-wrapper)
- [Tests](#tests)
  - [Core Test](#core-test)
  - [OpenSSL Test](#openssl-test)
- [Example](#example-1)
- [Benchmark](#benchmark)
  - [callback style C API](#callback-style-c-api)
  - [pull style C API](#pull-style-c-api)
  - [LuaJIT](#luajit)
  - [Lua 5.4.4](#lua-544)
- [Projects](#projects)
- [Thanks](#thanks)

# About

m_net was a  [single file](https://github.com/lalawue/m_net/blob/master/src/mnet_core.c) cross platform network library,
provide a simple and efficient interface for covenient use.

Also support Lua/LuaJIT with pull style API interface.

Support Linux/MacOS/FreeBSD/Windows, using epoll/kqueue/[wepoll](https://github.com/piscisaureus/wepoll) underlying.

Please use gmake to build demo under FreeBSD.



# Features

- with TCP/UDP support
- nonblocking & event driven interface
- using epoll/kqueue/[wepoll](https://github.com/piscisaureus/wepoll) in Linux/MacOS/FreeBSD/Windows
- support Lua/LuaJIT with pull style API
- buildin timer event
- simple API in C++ wrapper
- support SSL/TLS with [OpenSSL extension](https://github.com/lalawue/m_net/tree/master/extension/openssl/)
- extension skeleton on top of bare socket TCP/UDP


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
         while (mnet_poll(1000000) > 0) {
         }
         mnet_fini();
      }
   }
   return 0;
}
```

In the other hand, the C interface with more flexible options.



# Lua/LuaJIT Wrapper

recommand using [LuaRocks](https://luarocks.org/) to build, then run [examples/chann_web.lua](https://github.com/lalawue/m_net/blob/master/examples/chann_web.lua)

```sh
$ luarocks make
$ lua examples/chann_web.lua
```

open browser to visit 'http://127.0.0.1:8080' and get 'hello, world !', and you will get browser's request infomation in terminal side.


# DNS query

add DNS query interface with LuaJIT binding, in `extension/mdns_utils` dir, default query `www.baidu.com`

```sh
$ luajit examples/test_mdns.lua www.github.com www.sina.com
using LuaJIT 2.1.0-beta3
---
query   www.baidu.com   14.215.177.39
query   www.github.com  13.250.177.223
query   www.sina.com    113.96.179.243
```

# OpenSSL support

provide [OpenSSL extension](https://github.com/lalawue/m_net/tree/master/extension/openssl/) to wrap a SSL/TLS chann, two steps to create a TLS chann in C:

```c
mnet_tls_config(SSL_CTX *ctx);
chann_t *n = mnet_chann_open(CHANN_TYPE_TLS);
// use chann to listen/accept/connect/recv/send TLS data like normal TCP STREAM
```

## Example

first build with openssl extension with command below, I install openssl with brew under MacOS.

```
$ export MNET_OPENSSL_DIR=/usr/local/Cellar/openssl@1.1/1.1.1k/
$ export DYLD_LIBRARY_PATH=/usr/local/Cellar/openssl@1.1/1.1.1k/lib/
$ make openssl
```

then run server

```
$ ./build/tls_svr
```

and client

```
$ ./build/tls_cnt
```

get testing code and readme under [examples/openssl/](https://github.com/lalawue/m_net/tree/master/examples/openssl/) dir.

## LuaJIT TLS wrapper

ffi-mnet under `extension/luajit/` also support OpenSSL after you build libary support and export DYLD_LIBRARY_PATH.

```sh
$ export LUA_PATH=./extension/luajit/?.lua
$ export LUA_CPATH=./build/?.so
$ luajit examples/openssl/tls_web.lua
```

then you can visit `https://127.0.0.1:8080` with browser, or

```sh
$ curl -k https://127.0.0.1:8080
hello, world !
```

Details in [tls_web_cnt.lua](https://github.com/lalawue/m_net/tree/master/examples/openssl/tls_web_cnt.lua) or [tls_web_svr.lua](https://github.com/lalawue/m_net/tree/master/examples/openssl/tls_web_svr.lua)

# Tests

only point to point testing, with callback/pull Style API, no unit test right now.

## Core Test

C/C++ core test in [test](https://github.com/lalawue/m_net/tree/master/test) dir.

- test_reconnect: test multi channs (default 256 with 'ulimits -n') in client connect/disconnect server 5 times
- test_rwdata: client send sequence data with each byte from 0 ~ 255, and wanted same data back, up to 1 GB
- test_timer: test client invoke with random seconds, send data to server, close when running duration over 10 seconds

## OpenSSL Test

OpenSSL test in [test/openssl/](https://github.com/lalawue/m_net/tree/master/test/openssl) dir, only provide pull style test.

- tls_test_reconnect: test multi channs (default 256 with 'ulimits -n') in client connect/disconnect server 5 times
- tls_test_rwdata: client send sequence data with each byte from 0 ~ 255, and wanted same data back, up to 1 GB


# Example

take simple example above, or details in [examples](https://github.com/lalawue/m_net/tree/master/examples).

including UDP/TCP, C/C++, callback/pull style API, timer event examples, also prvode Lua/LuaJIT one as a tiny web server.




# Benchmark

Intel 12700F 64G, server and wrk in same PC.

## callback style C API

benchmark for `examples/chann_web_callback_style.cpp`

```
$ make callback
$ ./build/chann_web_callback_style.out
```

```
$ wrk -t8 -c200 --latency http://127.0.0.1:8080
Running 10s test @ http://127.0.0.1:8080
  8 threads and 200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.69ms    0.87ms  11.03ms   71.33%
    Req/Sec     9.20k     1.39k   12.94k    68.81%
  Latency Distribution
     50%    2.66ms
     75%    3.18ms
     90%    3.62ms
     99%    6.17ms
  739951 requests in 10.10s, 7.40GB read
  Socket errors: connect 0, read 1401, write 0, timeout 0
Requests/sec:  73255.33
Transfer/sec:    750.45MB
```

## pull style C API

benchmark for `examples/chann_web_pull_style.c`

```
$ make pull
$ ./build/chann_web_pull_style.out
```

```
$ wrk -t8 -c200 --latency http://127.0.0.1:8080
Running 10s test @ http://127.0.0.1:8080
  8 threads and 200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.52ms  721.19us  11.40ms   67.74%
    Req/Sec     9.81k     1.82k   15.47k    65.30%
  Latency Distribution
     50%    2.57ms
     75%    2.83ms
     90%    3.47ms
     99%    4.04ms
  787535 requests in 10.10s, 7.88GB read
  Socket errors: connect 0, read 1062, write 0, timeout 0
Requests/sec:  77967.80
Transfer/sec:    798.73MB
```

## LuaJIT

benchmark for `luajit examples/chann_web.lua`, first create `mnet.so`

```
$ make lib
$ cp build/libmnet.* build/mnet.so
$ export LD_LIBRARY_PATH=$PWD/build
$ export DYLD_LIBRARY_PATH=$PWD/build
$ export LUA_CPATH=./build/?.so
$ export LUA_PATH=./extension/luajit/?.lua
$ luajit examples/chann_web.lua
```

```
$ wrk -t8 -c200 --latency http://127.0.0.1:8080
Running 10s test @ http://127.0.0.1:8080
  8 threads and 200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.16ms  495.30us   5.83ms   74.90%
    Req/Sec    11.61k     1.47k   26.65k    87.30%
  Latency Distribution
     50%    2.22ms
     75%    2.49ms
     90%    2.67ms
     99%    3.07ms
  927741 requests in 10.10s, 8.76GB read
  Socket errors: connect 0, read 103, write 0, timeout 0
Requests/sec:  91857.05
Transfer/sec:      0.87GB
```

## Lua 5.4.4

```
$ lua examples/chann_web.lua
```

```
$ wrk -t8 -c200 --latency http://127.0.0.1:8080
Running 10s test @ http://127.0.0.1:8080
  8 threads and 200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.61ms    0.95ms  79.69ms   94.69%
    Req/Sec     9.68k     0.99k   16.42k    67.79%
  Latency Distribution
     50%    2.67ms
     75%    2.97ms
     90%    3.08ms
     99%    3.40ms
  774119 requests in 10.10s, 7.31GB read
  Socket errors: connect 0, read 67, write 0, timeout 0
Requests/sec:  76648.12
Transfer/sec:    740.77MB
```



# Projects

- [cincau](https://github.com/lalawue/cincau): a fast, minimalist and high configurable framework for LuaJIT based on m_net or openresty (nginx)
- [rpc_framework](https://github.com/lalawue/rpc_framework): LuaJIT base network RPC framework for MacOS/Linux/FreeBSD/Windows.



# Thanks

Thanks NTP source code from https://github.com/edma2/ntp, author: Eugene Ma (edma2)
