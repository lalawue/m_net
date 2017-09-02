
# About

m_net was a cross platform network library, provide a simple and
efficient interface for covenient use.

Support MacOS/Linux/Windows, using kqueue/epoll/select underlying.




# Features

- simple API in C++ wrapper
- with TCP/UDP support
- nonblocking & event driven interface
- using kqueue/epoll/select in MacOS/Linux/Windows




# Usage & Example

It's very convenience for use, an echo server below:

```
   Chann echoSvr("tcp");
   echoSvr.listen(argv[1]);
   
   echoSvr.setEventHandler([](Chann *self, Chann *accept, mnet_event_type_t event) {
         if (event == MNET_EVENT_ACCEPT) {
            cout << "svr accept cnt " << accept << endl;
         }
      });

   ChannDispatcher::startEventLoop();
```

the nested code need Closure support, with environment:

- Apple LLVM version 8.1.0 (clang-802.0.42)
- g++ (Ubuntu/Linaro 4.6.3-1ubuntu5) 4.6.3

In the other hand, the C interface with more flexible options.

Feel free to drop plat_net.[ch] to your project, more example and test
case can be found in relative dir.

A more complex demo, the [m_tunnel](https://github.com/lalawue/m_tunnel)
project also using this library to provide a secure socks5 interface tcp
connection between local <-> remote side.




# Thanks

Thanks NTP source code from https://github.com/edma2/ntp, author: Eugene Ma (edma2)