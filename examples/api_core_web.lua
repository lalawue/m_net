-- 
-- Copyright (c) 2017 lalawue
-- 
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--

package.cpath = package.cpath .. ";../build/?.so;./build/?.so;"
package.path = package.path .. ";../extension/lua/?.lua;./extension/lua/?.lua;"

local ipport = ...

local mnet = require("mnet.core")

local function on_chann_msg(emsg, n, r)

   if emsg == "accept" then
      if r then
         mnet.chann_set_cb(r, on_chann_msg)
      end

   elseif emsg == "recv" then
      
      local str = mnet.chann_recv(n)        -- recv anything
      print(str)

      local toast = "hello, world !\n"
      
      local data = 'HTTP/1.1 200 OK\r\n'
         .. 'Server: MNetCoreWeb/0.0\r\n'
         .. 'Content-Type: text/plain\r\n'
         .. string.format("Content-Length: %d\r\n\r\n", toast:len())
         .. toast

      mnet.chann_active_event(n, "send", true)      
      mnet.chann_send(n, data)

   elseif emsg == "disconnect" or emsg == "send" then
      print(string.format("[D] close chann with emsg: %s\n", emsg))
      mnet.chann_close(n)
   end
end


-- params
if ipport then
   mnet.init()

   local n = mnet.chann_open("tcp")

   if n and mnet.chann_listen(n, ipport, 2) then
      print("[D] listen to ", ipport)
      mnet.chann_set_cb(n, on_chann_msg);
      while true do
         mnet.poll( 10000 )
      end
   else
      print("fail to create mnet chann !")
   end

   mnet.fini()
else
   print("run as 127.0.0.1:8080")
end
