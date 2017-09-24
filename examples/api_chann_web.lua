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

local tcpServ = Chann:extend()     -- create lua structure
local tcpCnt = Chann:extend()

-- client
function tcpCnt:onEvent(emsg, remote)
   if emsg == "recv" then
      
      local buf = self:recv()
      print("recv:\n", buf)

      local toast = "hello, world !\n" 
      local data = 'Content-Type: text/plain\r\n'
         .. string.format("Content-Length: %d\r\n\r\n", toast:len())
         .. toast

      self:activeEvent("send", true)
      self:send( data )

   elseif emsg == "send" or emsg == "disconnect" then
      self:close()
   end
end

-- server
function tcpServ:onEvent(emsg, remote)
   if emsg == "accept" then
      remote.onEvent = tcpCnt.onEvent
   end
end

if ipport then
   tcpServ:open("tcp")
   if tcpServ:listen( ipport ) then
      print("listen to", ipport)
   end
   Chann:globalDispatch( 100 )
else
   print("run with '127.0.0.1:8080'")
end
