
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

It's very convenience for use, an echo server example:

```cpp
// subclass Chann to handle event
class CntChann : public Chann {
public:
   CntChann(Chann *c) {
      updateChann(c, NULL);
   }

   // implement virtual defaultEventHandler
   void defaultEventHandler(Chann *accept, mnet_event_type_t event) {

      if (event == MNET_EVENT_RECV) {

         char buf[256] = { 0 };
         memset(buf, 0, 256);

         int ret = recv(buf, 256);
         if (ret > 0) {
            cout << "svr_recv: " << buf << endl;
            send(buf, ret);
         } else {
            delete this;
         }
      }
      else if (event == MNET_EVENT_DISCONNECT) {
         cout << "cnt disconnect " << this << endl;
         delete this;              // release accepted chann
      }
   }
};


int
main(int argc, char *argv[]) {

   if (argc < 2) {
      cout << argv[0] << ": 'svr_ip:port'" << endl;
      return 0;
   } else {
      cout << "svr start listen: " << argv[1] << endl;
   }

   Chann echoSvr("tcp");
   echoSvr.listen(argv[1]);
   
   echoSvr.setEventHandler([](Chann *self, Chann *accept, mnet_event_type_t event) {
         if (event == MNET_EVENT_ACCEPT) {
            CntChann *cnt = new CntChann(accept);
            cout << "svr accept cnt " << cnt << endl;
            delete accept;
         }
      });

   ChannDispatcher::startEventLoop();

   return 0;
}
```

the nested code need Closure support, with environment:

- Apple LLVM version 8.1.0 (clang-802.0.42)
- g++ (Ubuntu/Linaro 4.6.3-1ubuntu5) 4.6.3


and the client side example:

```cpp
static void
_getUserInputThenSend(Chann *self) {
   string input;
   cin >> input;
   if (input.length() > 0) {
      self->send((void*)input.c_str(), input.length());
   }
}

// external event handler
static void
_senderEventHandler(Chann *self, Chann *accept, mnet_event_type_t event) {
   if (event == MNET_EVENT_CONNECTED) {
      cout << "cnt connected !" << endl;
      _getUserInputThenSend(self);
   }
   else if (event == MNET_EVENT_RECV) {
      char buf[256] = { 0 };
      memset(buf, 0, 256);
      int ret = self->recv(buf, 256);
      if (ret > 0) {
         cout << "cnt_recv: " << buf << endl;
         _getUserInputThenSend(self);
      }
      else {
         ChannDispatcher::stopEventLoop();
      }
   }
   else if (event == MNET_EVENT_DISCONNECT) {
      ChannDispatcher::stopEventLoop();
   }
}

int
main(int argc, char *argv[]) {
   if (argc < 2) {

      cout << argv[0] << ": 'cnt_ip:port'" << endl;

   } else {

      cout << "cnt try connect " << argv[1] << "..." << endl;

      Chann sender("tcp");
      sender.setEventHandler(_senderEventHandler);
      sender.connect(argv[1]);

      ChannDispatcher::startEventLoop();

      sender.disconnect();
   }

   return 0;
}
```

In the other hand, the C interface with more flexible options.

Feel free to drop plat_net.[ch] to your project, more example and test
case can be found in relative dir.

A more complex demo, the [m_tunnel](https://github.com/lalawue/m_tunnel)
project also using this library to provide a secure socks5 interface tcp
connection between local <-> remote side.




# Thanks

Thanks NTP source code from https://github.com/edma2/ntp, author: Eugene Ma (edma2)