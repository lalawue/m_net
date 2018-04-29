-- 
-- Copyright (c) 2017 lalawue
-- 
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--

local mnet = require("mnet.core")
mnet.init()                  -- init mnet


local Chann = {
   -- .chann: chann of mnet.core
   -- .streamType: 'tcp', 'udp' or 'broadcast'
}
Chann.__index = Chann
Chann.__gc = function(c)
   c:close()
end


-- all channs for C level callback dispatch
local MNetChannInstance = {
   -- [ c_insteance ] = lua_instance   
}


-- register to instance table
local function MNetChannRegister(lua_chann, c_chann)
   if lua_chann and c_chann then
      lua_chann.chann = c_chann
      MNetChannInstance[ c_chann ] = lua_chann
   end
end



-- from rxi 'classic', https://github.com/rxi/classic/blob/master/classic.lua
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


local RemoteChann = Chann:extend()


-- global event dispatcher for C level
function MNetChannDispatchEvent(emsg, n, r)

   local chann = MNetChannInstance[n]
   if chann then

      local remote = nil
      if r then
         remote = RemoteChann()
         remote.streamType = chann.streamType
         MNetChannRegister(remote, r)
      end

      chann:onEvent(emsg, remote)
   end
end


-- create a chann instance
function Chann:__call(...)
   local obj = setmetatable({}, self)
   return obj
end

-- create mnet instance, register to global table,
-- streamType: 'tcp', 'udp', 'broadcast'
function Chann:open( streamType )
   if streamType == "tcp" or
      streamType == "udp" or
      streamType == "broadcast"   
   then
      self.streamType = streamType
      if not self.chann then
         local n = mnet.chann_open( self.streamType )
         MNetChannRegister(self, n)
      end
      
   end
end

-- close and release C level resources
function Chann:close()
   if self.chann then
      MNetChannInstance[self.chann] = nil
      mnet.chann_close(self.chann)
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

-- mnet poll, only need one poll in one process
function Chann:pollEvent( microseconds )
   return mnet.poll( microseconds )
end

-- finalize all chann instance
function Chann:finalizeAllChann()
   mnet.fini()
   MNetChannInstance = nil
   RemoteChann = nil
   Chann = nil
end

return Chann
