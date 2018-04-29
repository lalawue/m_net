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

local TcpServ = Chann:extend()  -- create new class base on Chann
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
      -- 
      -- mnet-chann stored 'remote' instance intenal, here only replace
      -- onEvent implement as TcpAgent's
      -- 
      remote.onEvent = TcpAgent.onEvent
   end
end


if ipport then
   
   local svr = TcpServ()        -- create an instance
   
   svr:open("tcp")
   if svr:listen( ipport ) then
      print("listen to", ipport)
   end

   while true do
      svr:globalPoll( 100000 )        -- one poll enough
   end
else
   print("run with '127.0.0.1:8080'")
end
