-- 
-- Copyright (c) 2019 lalawue
-- 
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--

package.path = package.path .. ";../extension/luajit/?.lua;./extension/luajit/?.lua;"

local Core = require("ffi_mnet")
local ipport = ...

if not ipport then
   print("chann_web_jit.lua 'IP:PORT'")
   os.exit(0)
end

Core.init()

local addr = Core.parseIpPort(ipport)
print("open svr in ", addr.ip, addr.port)

-- default 256, 256
Core.setBufSize(32, 1024)

-- open tcp stream
local svr = Core.openChann("tcp")
svr:listen(addr.ip, addr.port, 100)

-- client callback function
local function clientCallback(self, eventName, accept)
   --print("eventName: ", eventName)
   if eventName == "event_recv" then
      local buf = self:recv()
      print("recv:\n", buf)

      local toast = "hello, world !\n"
      
      local data = 'HTTP/1.1 200 OK\r\n'
         .. 'Server: MnetChannWeb/0.1\r\n'
         .. 'Content-Type: text/plain\r\n'
         .. string.format("Content-Length: %d\r\n\r\n", toast:len())
         .. toast

      -- receive 'event_send' when send buffer was empty
      self:activeEvent("event_send", true)
      self:send( data )
      
   elseif eventName == "event_disconnect" or eventName == "event_send" then
      if self.client_has_send then
         local addr = self:addr()
         print("---")
         print("client ip: " .. addr.ip .. ":" .. addr.port)
         print("state: " .. self:state())
         print("bytes send: " .. self:sendByes())
         self:close()
      else
         self:send("\n     \n     \n     \n")
         self.client_has_send = true
      end
   end
end

-- server callback function
svr:setCallback(function(self, eventName, accept)
      if eventName == "event_accept" and accept then
         local addr = accept:addr()
         print("---")  
         print("accept client from " .. addr.ip .. ":" .. addr.port)
         accept:setCallback(clientCallback) -- client callback function
      end
end)

while Core.poll(1000000) do
   -- do nothing
end

Core.fini()

