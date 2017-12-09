

[![Powered][1]][2]  [![MIT licensed][3]][4]  [![Build Status][5]][6]  [![Downloads][7]][8]

[1]: https://img.shields.io/badge/KCP-Powered-blue.svg
[2]: https://github.com/skywind3000/kcp

[3]: https://img.shields.io/badge/license-MIT-blue.svg
[4]: LICENSE

[5]: https://travis-ci.org/lalawue/m_net.svg?branch=master
[6]: https://travis-ci.org/lalawue/m_net

[7]: https://img.shields.io/github/downloads/lalawue/m_kcptun/total.svg?maxAge=1800
[8]: https://github.com/lalawue/m_kcptun/releases


# About

m_net was a cross platform network library, provide a simple and
efficient interface for covenient use.

Support MacOS/Linux/Windows, using kqueue/epoll/select underlying.




# Features

- simple API in C++ wrapper
- with TCP/UDP support
- nonblocking & event driven interface
- using kqueue/epoll/select in MacOS/Linux/Windows
- support Lua binding




# Server 

It's very convenience for use, an echo server example, with CPP wrapper:

```cpp
// subclass Chann to handle event
class CntChann : public Chann {
public:
   CntChann(Chann *c) { channUpdate(c, NULL); }

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
                  cnt->channSend((void*)("Welcome to echoServ\n"), 20);
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

Feel free to drop mnet_core.[ch] to your project, more example and test
case can be found in relative dir.



# Lua

try '$make lua' then run lua examples:

```
lua examples/api_chann_web.lua '127.0.0.1:1080'
```

use 'curl -i 127.0.0.1:8080' to get the HTTP request header





# Projects

- [m_tunnel](https://github.com/lalawue/m_tunnel): provide a secure
  socks5 interface tcp connection between local <-> remote side.

- [m_kcptun](https://github.com/lalawue/m_kcptun): provide a speedup TCP
  connection between local <-> remote, with [KCP](https://github.com/skywind3000/kcp) underlying




# Thanks

Thanks NTP source code from https://github.com/edma2/ntp, author: Eugene Ma (edma2)
