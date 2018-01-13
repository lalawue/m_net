-- 
-- Copyright (c) 2017 lalawue
-- 
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--

package.cpath = package.cpath .. ";../build/?.so;./build/?.so;"
package.path = package.path .. ";../extension/lua/?.lua;./extension/lua/?.lua;"

local ipport = ...

local Chann = require("mnet-chann")

local TcpServ = Chann:extend()     -- create lua structure
local TcpAgent = Chann:extend()

-- agent
function TcpAgent:onEvent(emsg, remote)
   if emsg == "recv" then
      
      local buf = self:recv()
      print("recv:\n", buf)

      local toast = "hello, world !\n"
      
      local data = 'HTTP/1.1 200 OK\r\n'
         .. 'Server: MNetChannWeb/0.0\r\n'
         .. 'Content-Type: text/plain\r\n'
         .. string.format("Content-Length: %d\r\n\r\n", toast:len())
         .. toast

      self:send( data )

   elseif emsg == "disconnect" then
      self:close()
   end
end

-- server
function TcpServ:onEvent(emsg, remote)
   if emsg == "accept" then
      remote.onEvent = TcpAgent.onEvent
   end
end

if ipport then
   TcpServ:open("tcp")
   if TcpServ:listen( ipport ) then
      print("listen to", ipport)
   end
   Chann:globalDispatch( 100000 )
else
   print("run as '127.0.0.1:8080'")
end
