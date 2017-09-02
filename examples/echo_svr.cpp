/* 
 * Copyright (c) 2017 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <iostream>
#include "mnet_wrapper.h"

#ifdef EXAMPLE_ECHO_SVR

using std::cout;
using std::endl;
using std::string;
using mnet::Chann;
using mnet::ChannDispatcher;


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

#endif  /* EXAMPLE_ECHO */
