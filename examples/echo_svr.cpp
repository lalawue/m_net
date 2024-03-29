/*
 * Copyright (c) 2017 lalawue
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <iostream>
#include <unistd.h>
#include "mnet_wrapper.h"

#ifdef EXAMPLE_ECHO_SVR_CPP

using mnet::Chann;
using mnet::ChannDispatcher;
using std::cout;
using std::endl;
using std::string;

// subclass Chann to handle event
class CntChann : public Chann
{
public:
   CntChann(Chann *c) : Chann(c) {}

   // implement virtual defaultEventHandler
   void defaultEventHandler(Chann *accept, chann_event_t event, int err)
   {

      if (event == CHANN_EVENT_RECV)
      {
         int ret = channRecv(m_buf, 256);
         channSend(m_buf, ret);
      }
      else if (event == CHANN_EVENT_DISCONNECT)
      {
         usleep(1000);
         cout << "svr disconnect cnt with chann " << this->myAddr().addrString << endl;
         delete this; // release chann
      }
   }
   char m_buf[256];
};

int main(int argc, char *argv[])
{
   string ipaddr = argc > 1 ? argv[1] : "127.0.0.1:8090";

   Chann echoSvr("tcp");

   if (echoSvr.channListen(ipaddr, 100))
   {
      cout << "svr version: " << ChannDispatcher::version() << endl;
      cout << "svr start listen: " << ipaddr << endl;

      // server will receive timer event every 5 second
      echoSvr.channActiveEvent(CHANN_EVENT_TIMER, 5 * MNET_MILLI_SECOND);

      echoSvr.setEventHandler([](Chann *self, Chann *accept, chann_event_t event, int err)
                              {
                                 if (event == CHANN_EVENT_ACCEPT) {
                                    CntChann *cnt = new CntChann(accept);
                                    char welcome[] = "Welcome to echoServ\n";
                                    cnt->channSend((void*)welcome, sizeof(welcome));

                                    cout << "svr accept cnt with chann " << cnt->myAddr().addrString << endl;
                                    delete accept;
                                 } else if (event == CHANN_EVENT_TIMER) {
                                    cout << "svr current time: " << ChannDispatcher::currentTime() << " ms " << endl;
                                 } });

      while (ChannDispatcher::pollEvent(0.1 * MNET_MILLI_SECOND) > 0)
      {
      }
   }
   return 0;
}

#endif // EXAMPLE_ECHO_SVR_CPP
