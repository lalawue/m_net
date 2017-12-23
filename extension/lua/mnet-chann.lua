-- 
-- Copyright (c) 2017 lalawue
-- 
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--

local mnet = require("mnet.core")
mnet.init()                  -- init mnet

local ChannTable = {}        -- all channs for C level callback dispatch

local Chann = {
   -- .chann: chann of mnet.core
   -- .streamType: 'tcp', 'udp' or 'broadcast'
}
Chann.__index = Chann

-- global dispatcher from C level
local function onChannEvent(emsg, n, r)

   local chann = ChannTable[n]
   if chann then

      local remote = nil
      if r then
         remote = Chann:extend()
         remote:setupCoreInfo(r, chann.streamType)
      end

      chann:onEvent(emsg, remote)
   end
end

-- register chann to global chann table, for later dispatch
function Chann:setupCoreInfo(n, streamType)
   if n and streamType then
      self.chann = n
      self.streamType = streamType
      ChannTable[n] = self
      mnet.chann_set_cb(n, onChannEvent)
   end
end



-- 
-- Public Interface
-- 

-- from 'rxi's classic.lua'
function Chann:extend()
   local cls = {}
   for k, v in pairs(self) do
      if k:find("__") == 1 then
         cls[k] = v
      end
   end
   cls.__index = cls
   cls.super = self
   setmetatable(cls, self)
   return cls
end

-- streamType: 'tcp', 'udp', 'broadcast'
function Chann:open( streamType )
   if not self.chann then
      local n = mnet.chann_open( streamType )
      self:setupCoreInfo(n, streamType)
   end
end

-- close and release C level resources
function Chann:close()
   if self.chann then
      mnet.chann_close(self.chann)
      ChannTable[self.chann] = nil
      self.chann = nil
      self.stremType = nil
      self = nil
   end
end

-- listen('127.0.0.0:8080')
function Chann:listen(ipport, backlog)
   return mnet.chann_listen(self.chann, ipport, backlog or 8)
end

-- connect('127.0.0.1:8080')
function Chann:connect(ipport)
   return mnet.chann_connect(self.chann, ipport)
end

function Chann:disconnect()
   mnet.chann_disconnect(self.chann, ipport)
end

-- return string
function Chann:recv()
   return mnet.chann_recv(self.chann)
end

-- send string
function Chann:send( buf )
   return mnet.chann_send(self.chann, buf)
end

-- @emsg: 'recv', 'send', 'accept', 'connected', 'disconnect'
-- @remote: accepted remote Chann, remote:close() to reject
-- 
function Chann:onEvent(emsg, remote)
end

-- set bufsize before connect or listen
function Chann:setBufSize( bufsize )
   mnet.chann_set_bufsize(self.chann, bufsize)
end

-- get cached in bytes
function Chann:cached()
   return mnet.chann_cached(self.chann)
end

-- return as '127.0.0.1:8080'
function Chann:addr()
   return mnet.chann_addr(self.chann)
end

-- state: 'closed', 'disconnect', 'connecting', 'connected', 'listening'
function Chann:state()
   return mnet.chann_state(self.chann)
end

function Chann:bytesReceived()
   return mnet.chann_bytes(self.chann, false)
end

function Chann:bytesSended()
   return mnet.chann_bytes(self.chann, true)
end

-- active 'send' event when send buffer empty
function Chann:activeEvent( emsg, isActive )
   mnet.chann_active_event(self.chann, emsg, isActive)
end

-- global dispatch channs
function Chann:globalDispatch( microseconds )
   if self == Chann then
      while true do
         mnet.poll( microseconds )
      end
   end
end

return Chann
