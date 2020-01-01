-- 
-- Copyright (c) 2019 lalawue
-- 
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--

package.path = package.path .. ";../extension/luajit/?.lua;./extension/luajit/?.lua;"

local mnet = require("ffi_mnet")
local ipport = ...

if not ipport then
   print("chann_web_jit.lua 'IP:PORT'")
   os.exit(0)
else
   mnet.init(1)

   local chann = mnet.openChann("tcp")
   assert(chann)

   local addr = mnet.parse_ipport(ipport)

   chann:listen(addr.ip, addr.port, 2)
   print("open svr in ", addr.ip, addr.port)

   local function clientCallback(self, eventName, acceptChann)
      if eventName == "event_recv" then
         local buf = self:recv()
         print("recv:\n", buf)

         local toast = "hello, world !\n"
         
         local data = 'HTTP/1.1 200 OK\r\n'
            .. 'Server: MNetChannWeb/0.0\r\n'
            .. 'Content-Type: text/plain\r\n'
            .. string.format("Content-Length: %d\r\n\r\n", toast:len())
            .. toast
         self:send( data )
         
      elseif eventName == "event_disconnect" then
         local addr = self:addr()
         print("---")
         print("client ip: " .. addr.ip .. ":" .. addr.port)
         print("state: " .. self:state())
         print("bytes send: " .. self:bytes(1))
         self:close()
         chann = nil
      end
   end

   chann:setCallback(function(self, eventName, acceptChann)
         if eventName == "event_accept" then
            local addr = acceptChann:addr()
            print("---")         
            print("accept client from " .. addr.ip .. ":" .. addr.port)
            acceptChann:setRecvBufSize(1024)
            acceptChann:setCallback(clientCallback)
         end
   end)

   while mnet.poll(1000000) do
      collectgarbage("collect")
   end

   mnet.fini()
end
