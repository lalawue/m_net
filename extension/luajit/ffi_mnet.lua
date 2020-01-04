-- 
-- Copyright (c) 2019 lalawue
-- 
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--

local ffi = require "ffi"

ffi.cdef[[
typedef enum {
   CHANN_TYPE_STREAM = 1,
   CHANN_TYPE_DGRAM,
   CHANN_TYPE_BROADCAST,
} chann_type_t;

typedef enum {
   CHANN_STATE_CLOSED = 0,
   CHANN_STATE_DISCONNECT,
   CHANN_STATE_CONNECTING,
   CHANN_STATE_CONNECTED,
   CHANN_STATE_LISTENING,
} chann_state_t;

typedef enum {
   CHANN_EVENT_RECV = 1,     /* socket has data to read */
   CHANN_EVENT_SEND,         /* socket send buf empty, inactive default */
   CHANN_EVENT_ACCEPT,       /* socket accept */
   CHANN_EVENT_CONNECTED,    /* socket connected */
   CHANN_EVENT_DISCONNECT,   /* socket disconnect when EOF or error */
} chann_event_t;

typedef struct s_mchann chann_t;

typedef struct {
   chann_event_t event;         /* event type */
   int err;                     /* errno */
   chann_t *n;                  /* chann to emit event */
   chann_t *r;                  /* chann accept from remote */
   void *opaque;                /* opaque in set_cb; next msg for pull style */
} chann_msg_t;

typedef struct {
   int chann_count;             /* -1 for error */
   chann_msg_t *msg;            /* msg for pull style result */
} poll_result_t;

typedef struct {
   int ret;                     /* recv/send data length */
   chann_msg_t *msg;            /* msg for pull style result */
} rw_result_t;

typedef struct {
   char ip[16];
   int port;
} chann_addr_t;

/* init before use chann */
int mnet_init(int); /* 0: callback style
                       1: pull style api*/
void mnet_fini(void);
int mnet_report(int level);     /* 0: chann_count 
                                   1: chann_detail */

poll_result_t* mnet_poll(int microseconds); /* dispatch chann event,  */



/* channel */
chann_t* mnet_chann_open(chann_type_t type);
void mnet_chann_close(chann_t *n);
chann_type_t mnet_chann_type(chann_t *n);

int mnet_chann_listen(chann_t *n, const char *host, int port, int backlog);

int mnet_chann_connect(chann_t *n, const char *host, int port);
void mnet_chann_disconnect(chann_t *n);

void mnet_chann_active_event(chann_t *n, chann_event_t et, int active);

rw_result_t* mnet_chann_recv(chann_t *n, void *buf, int len);
rw_result_t* mnet_chann_send(chann_t *n, void *buf, int len);

int mnet_chann_set_bufsize(chann_t *n, int bufsize);
int mnet_chann_cached(chann_t *n);

int mnet_chann_addr(chann_t *n, chann_addr_t*);

int mnet_chann_state(chann_t *n);
long long mnet_chann_bytes(chann_t *n, int be_send);



/* tools without init */
int mnet_resolve(char *host, int port, chann_type_t ctype, chann_addr_t*);
int mnet_parse_ipport(const char *ipport, chann_addr_t *addr);
]]

local mnet_core = ffi.load("mnet")

local mnet_poll = mnet_core.mnet_poll
local mnet_chann_open = mnet_core.mnet_chann_open
local mnet_chann_close = mnet_core.mnet_chann_close
local mnet_chann_type = mnet_core.mnet_chann_type
local mnet_chann_listen = mnet_core.mnet_chann_listen
local mnet_chann_connect = mnet_core.mnet_chann_connect
local mnet_chann_disconnect = mnet_core.mnet_chann_disconnect
local mnet_chann_active_event = mnet_core.mnet_chann_active_event
local mnet_chann_recv = mnet_core.mnet_chann_recv
local mnet_chann_send = mnet_core.mnet_chann_send
local mnet_chann_cached = mnet_core.mnet_chann_cached
local mnet_chann_addr = mnet_core.mnet_chann_addr
local mnet_chann_state = mnet_core.mnet_chann_state
local mnet_chann_bytes = mnet_core.mnet_chann_bytes

local ffinew = ffi.new
local fficopy = ffi.copy
local fficast = ffi.cast
local ffiistype = ffi.istype
local ffistring = ffi.string

local EventNamesTable = {
   "event_recv",
   "event_send",
   "event_accept",
   "event_connected",
   "event_disconnect",
}

local StateNamesTable = {
   "state_closed",
   "state_disconnect",
   "state_connecting",
   "state_connected",
   "state_listening",
}

local ChannTypesTable = {
   "tcp", "udp", "broadcast"
}

local AllChannsTable = {
   -- contain all channs    
}

-- chann
local Chann = {
   m_type = nil,                -- 'tcp', 'udp', 'broadcast'
   m_chann = nil,               -- chann_t of ctype
   m_bufsize = 256,             -- default recv buf size
   m_callback = nil,            -- callback
   m_addr = nil,
}
Chann.__index = Chann

local Core = {
   -- mnet core 
}

function Core.init()
   mnet_core.mnet_init(1)
end

function Core.fini()
   mnet_core.mnet_fini()
end

function Core.report(level)
   mnet_core.mnet_report(level)
end

function Core.poll(microseconds)
   local result = ffinew("poll_result_t *")   
   result = mnet_poll(microseconds)
   if result.chann_count < 0 then
      return -1
   elseif result.chann_count > 0 then
      local msg = result.msg
      while msg ~= nil do
         local accept = nil
         if msg.r ~= nil then
            accept = {}
            setmetatable(accept, Chann)
            accept.m_chann = msg.r
            accept.m_type = ChannTypesTable[tonumber(mnet_chann_type(msg.r))]
            AllChannsTable[tostring(msg.r)] = accept
         end
         local chann = AllChannsTable[tostring(msg.n)]
         if chann and chann.m_callback then
            chann.m_callback(chann, EventNamesTable[tonumber(msg.event)], accept)
         end
         if msg.opaque ~= nil then
            msg = ffinew("chann_msg_t *", msg.opaque)
         else
            break
         end
      end
   end
   return result.chann_count
end

function Core.resolve(host, port, chann_type)
   local addr = ffinew("chann_addr_t[1]")
   local ctype = ffinew("chann_type_t", tonumber(chann_type))
   local buf = ffinew("char[?]", host:len())
   fficopy(buf, host, host:len())
   if mnet_core.mnet_resolve(buf, tonumber(port), ctype, addr[0]) > 0 then
      local tbl = {}
      tbl.ip = ffistring(addr[0].ip, 16)
      tbl.port = tonumber(addr[0].port)
      return tbl
   else
      return nil
   end
end

function Core.parseIpPort(ipport)
   local addr = ffinew("chann_addr_t[1]")
   if mnet_core.mnet_parse_ipport(ipport, addr[0]) > 0 then
      local tbl = {}
      tbl.ip = ffistring(addr[0].ip, 16)
      tbl.port = tonumber(addr[0].port)
      return tbl
   else
      return nil
   end
end

--
-- Channs
-- 

function Core.openChann(chann_type)
   local chann = {}
   setmetatable(chann, Chann)
   assert(chann.m_bufsize)
   if chann_type == "broadcast" then
      chann.m_chann = mnet_chann_open(mnet_core.CHANN_TYPE_BROADCAST);
   elseif chann_type == "udp" then
      chann.m_chann = mnet_chann_open(mnet_core.CHANN_TYPE_DGRAM);
   else
      chann_type = "tcp"
      chann.m_chann = mnet_chann_open(mnet_core.CHANN_TYPE_STREAM);
   end
   chann.m_type = chann_type
   AllChannsTable[tostring(chann.m_chann)] = chann
   ffi.gc(chann.m_chann, mnet_chann_close)
   return chann
end

function Chann:close()
   AllChannsTable[tostring(self.m_chann)] = nil
   mnet_chann_close(self.m_chann)
   self.m_chann = nil
end

function Chann:channType()
   return self.m_type
end

function Chann:setRecvBufSize(size)
   self.m_bufsize = size
end

function Chann:listen(host, port, backlog)
   return mnet_chann_listen(self.m_chann, host, tonumber(port), backlog)
end

function Chann:connect(host, port)
   return mnet_chann_connect(self.m_chann, host, tonumber(port))
end

function Chann:disconnect()
   mnet_chann_disconnect(self.m_chann)
end

-- callback params should be (self, eventName, acceptChann) 
function Chann:setCallback(callback)
   self.m_callback = callback
end

function Chann:activeEvent(eventName, isActive)
   if eventName == "event_send" then
      local active = ffinew("int", isActive)
      mnet_chann_active_event(self.m_chann, mnet_core.CHANN_EVENT_SEND, active)
   end
end

function Chann:recv()
   local rw = ffinew("rw_result_t *")   
   local buf = ffinew("uint8_t[?]", self.m_bufsize)
   rw = mnet_chann_recv(self.m_chann, buf, ffinew("int", self.m_bufsize))
   if rw.ret <= 0 then
      if self.m_callback then
         self.m_callback(self, EventNamesTable[tonumber(rw.msg.event)], nil)
      end
      return nil
   else
      return ffistring(buf, rw.ret)
   end
end

function Chann:send(data)
   if type(data) ~= "string" then
      return false
   end
   local rw = ffinew("rw_result_t *")
   local buf = ffinew("uint8_t[?]", data:len())
   fficopy(buf, data, data:len())
   rw = mnet_chann_send(self.m_chann, buf, ffinew("int", data:len()))
   if rw.ret <= 0 then
      if self.m_callback then
         self.m_callback(self, EventNamesTable[tonumber(rw.msg.event)], nil)
      end
      return false
   else
      return true
   end
end

function Chann:setSocketBufSize(size)
   mnet_core.mnet_chann_set_bufsize(self.m_chann, tonumber(size))
end

function Chann:cachedSize()
   return mnet_chann_cached(self.m_chann)
end

function Chann:addr()
   if not self.m_addr then
      local addr = ffinew("chann_addr_t[1]")
      mnet_chann_addr(self.m_chann, addr[0])
      self.m_addr = {}
      self.m_addr.ip = ffistring(addr[0].ip, 16)
      self.m_addr.port = tonumber(addr[0].port)
   end
   return self.m_addr   
end

function Chann:state()
   return StateNamesTable[tonumber(mnet_chann_state(self.m_chann)) + 1]
end

function Chann:bytes(beSend)
   local be_send = ffinew("int", beSend or 0)
   return tonumber(mnet_chann_bytes(self.m_chann, be_send))
end

return Core
