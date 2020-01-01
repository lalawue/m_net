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

local mnet = {
   m_channs = {},               -- contain all channs 
}

local Chann = {
   m_type = nil,                -- 'tcp', 'udp', 'broadcast'
   m_chann = nil,               -- chann_t of ctype
   m_bufsize = 256,             -- default recv buf size
   m_callback = nil,            -- callback
   m_addr = nil,
}
Chann.__index = Chann

function mnet.init(api_style)
   mnet_core.mnet_init(api_style)
end

function mnet.fini()
   mnet_core.mnet_fini()
end

function mnet.report(level)
   mnet_core.mnet_report(level)
end

function mnet.poll(microseconds)
   local result = ffi.new("poll_result_t *");
   result = mnet_core.mnet_poll(microseconds)
   if result.chann_count < 0 then
      return false
   else
      local msg = result.msg
      while msg ~= nil do
         local accept = nil
         if msg.r then
            accept = {}
            setmetatable(accept, Chann)
            accept.m_chann = msg.r
            accept.m_type = mnet.channType(mnet_core.mnet_chann_type(msg.r))
            mnet.m_channs[tostring(msg.r)] = accept
         end
         local chann = mnet.m_channs[tostring(msg.n)]
         if chann and chann.m_callback then
            chann.m_callback(chann, mnet.eventName(msg.event), accept)
         end
         msg = msg.opaque
      end
      return true
   end
end

mnet.eventNames = {
   "event_recv",
   "event_send",
   "event_accept",
   "event_connected",
   "event_disconnect",
}

function mnet.eventName(chann_event)
   return mnet.eventNames[tonumber(chann_event)]
end

mnet.stateNames = {
   "state_closed",
   "state_disconnect",
   "state_connecting",
   "state_connected",
   "state_listening",
}

function mnet.stateName(chann_state)
   return mnet.stateNames[tonumber(chann_state) + 1]
end

mnet.channTypes = {
   "tcp", "udp", "broadcast"
}

function mnet.channType(chann_type)
   return mnet.channTypes[tonumber(chann_type)]
end

function mnet.resolve(host, port, chann_type)
   local addr = ffi.new("chann_addr_t[1]")
   local ctype = ffi.new("chann_type_t", tonumber(chann_type))
   if mnet_core.mnet_resolve(host, tonumber(port), ctype, addr[0]) > 0 then
      local tbl = {}
      tbl.ip = ffi.string(addr.ip, 16)
      tbl.port = tonumber(addr.port)
      return tbl
   else
      return nil
   end
end

function mnet.parse_ipport(ipport)
   local addr = ffi.new("chann_addr_t[1]")
   if mnet_core.mnet_parse_ipport(ipport, addr[0]) > 0 then
      local tbl = {}
      tbl.ip = ffi.string(addr[0].ip, 16)
      tbl.port = tonumber(addr[0].port)
      return tbl
   else
      return nil
   end
end

--
-- Channs

function mnet.openChann(chann_type)
   local chann = {}
   setmetatable(chann, Chann)
   assert(chann.m_bufsize)
   if chann_type == "broadcast" then
      chann.m_chann = mnet_core.mnet_chann_open(mnet_core.CHANN_TYPE_BROADCAST);
   elseif chann_type == "udp" then
      chann.m_chann = mnet_core.mnet_chann_open(mnet_core.CHANN_TYPE_DGRAM);
   else
      chann_type = "tcp"
      chann.m_chann = mnet_core.mnet_chann_open(mnet_core.CHANN_TYPE_STREAM);
   end
   chann.m_type = chann_type;   
   mnet.m_channs[tostring(chann.m_chann)] = chann
   ffi.gc(chann.m_chann, mnet_core.mnet_chann_close)
   return chann
end

function Chann:close()
   mnet.m_channs[tostring(self.m_chann)] = nil
   mnet_core.mnet_chann_close(self.m_chann)
   self.m_chann = nil
end

function Chann:setRecvBufSize(size)
   self.m_bufsize = size
end

function Chann:listen(host, port, backlog)
   return mnet_core.mnet_chann_listen(self.m_chann, host, tonumber(port), backlog)
end

function Chann:connect(host, port)
   return mnet_core.mnet_chann_connect(self.m_chann, host, tonumber(port))
end

function Chann:disconnect()
   mnet_core.mnet_chann_disconnect(self.m_chann)
end

-- callback params should be (self, eventName, acceptChann) 
function Chann:setCallback(callback)
   self.m_callback = callback
end

function Chann:activeEvent(eventName, isActive)
   if eventName == "chann_event_send" then
      local active = ffi.new("int", isActive)
      local event = ffi.new("chann_event_t", 2)
      mnet_core.mnet_chann_active_event(self.m_chann, event, active)
   end
end

function Chann:recv()
   local buf = ffi.new("uint8_t[?]", self.m_bufsize)
   local rw = ffi.new("rw_result_t *");   
   rw = mnet_core.mnet_chann_recv(self.m_chann, buf, ffi.new("int", self.m_bufsize))
   print("ret length ", rw.ret)
   if rw.ret <= 0 then
      if self.m_callback then
         self.m_callback(self, mnet.eventName(rw.msg.event), nil)
      end
      return nil
   else
      return ffi.string(buf, rw.ret)
   end
end

function Chann:send(data)
   if type(data) ~= "string" then
      return false
   end
   local buf = ffi.new("uint8_t[?]", data:len())
   local rw = ffi.new("rw_result_t *");
   ffi.copy(buf, data, data:len())
   rw = mnet_core.mnet_chann_send(self.m_chann, buf, ffi.new("int", data:len()))
   if rw.ret <= 0 then
      if self.m_callback then
         self.m_callback(self, mnet.eventName(rw.msg.event), nil)
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
   return mnet_core.mnet_chann_cached(self.m_chann)
end

function Chann:addr()
   if not self.m_addr then
      local addr = ffi.new("chann_addr_t[1]")
      mnet_core.mnet_chann_addr(self.m_chann, addr[0])
      self.m_addr = {}
      self.m_addr.ip = ffi.string(addr[0].ip, 16)
      self.m_addr.port = tonumber(addr[0].port)
   end
   return self.m_addr   
end

function Chann:state()
   return mnet.stateName(tonumber(mnet_core.mnet_chann_state(self.m_chann)))
end

function Chann:bytes(beSend)
   local be_send = ffi.new("int", beSend or 0)
   return tonumber(mnet_core.mnet_chann_bytes(self.m_chann, be_send))
end

return mnet


