/* 
 * Copyright (c) 2017 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <iostream>
#include "mnet_wrapper.h"

#ifdef EXAMPLE_ECHO_CNT

using std::cin;
using std::cout;
using std::endl;
using std::string;
using mnet::Chann;
using mnet::ChannDispatcher;

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
_senderEventHandler(Chann *self, Chann *accept, chann_event_t event) {
   if (event == CHANN_EVENT_CONNECTED) {
      cout << "cnt connected !" << endl;
      _getUserInputThenSend(self);
   }
   else if (event == CHANN_EVENT_RECV) {
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
   else if (event == CHANN_EVENT_DISCONNECT) {
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

#endif  /* EXAMPLE_ECHO */
