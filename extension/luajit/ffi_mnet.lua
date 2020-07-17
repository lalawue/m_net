--
-- Copyright (c) 2019 lalawue
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--

local ffi = require "ffi"

ffi.cdef [[
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

typedef struct s_chann_msg {
   chann_event_t event;         /* event type */
   int err;                     /* errno */
   chann_t *n;                  /* chann to emit event */
   chann_t *r;                  /* chann accept from remote */
   struct s_chann_msg *next;    /* modified as next msg for pull style */
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

/* micro seconds */
int64_t mnet_current(void);

poll_result_t* mnet_poll(uint32_t milliseconds); /* dispatch chann event,  */
chann_msg_t* mnet_result_next(poll_result_t *result); /* next msg */


/* channel */
chann_t* mnet_chann_open(chann_type_t type);
void mnet_chann_close(chann_t *n);

int mnet_chann_fd(chann_t *n);
chann_type_t mnet_chann_type(chann_t *n);

int mnet_chann_listen(chann_t *n, const char *host, int port, int backlog);

int mnet_chann_connect(chann_t *n, const char *host, int port);
void mnet_chann_disconnect(chann_t *n);

void mnet_chann_active_event(chann_t *n, chann_event_t et, int64_t active);

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
local mnet_result_next = mnet_core.mnet_result_next
local mnet_chann_open = mnet_core.mnet_chann_open
local mnet_chann_close = mnet_core.mnet_chann_close
local mnet_chann_fd = mnet_core.mnet_chann_fd
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
local mnet_current = mnet_core.mnet_current

local ffinew = ffi.new
local fficopy = ffi.copy
local fficast = ffi.cast
local ffistring = ffi.string

local EventNamesTable = {
    "event_recv",
    "event_send",
    "event_accept",
    "event_connected",
    "event_disconnect",
    "event_timer"
}

local StateNamesTable = {
    "state_closed",
    "state_disconnect",
    "state_connecting",
    "state_connected",
    "state_listening"
}

local ChannTypesTable = {
    "tcp",
    "udp",
    "broadcast"
}

local AllOpenedChannsTable = {} -- all opened channs

-- chann
local Chann = {
    m_type = nil, -- 'tcp', 'udp', 'broadcast'
    m_chann = nil, -- chann_t
    m_callback = nil, -- callback
    m_addr = nil -- self addr
}
Chann.__index = Chann

-- mnet core, shared by all channs
local Core = {
    m_recvsize = 256, -- default recv buf size
    m_sendsize = 256, -- default send buf size
}

-- C level local veriable
local _addr = ffinew("chann_addr_t[1]")
local _ctype = ffinew("chann_type_t", 0)

local _sendbuf = ffinew("uint8_t[?]", Core.m_sendsize)
local _recvbuf = ffinew("uint8_t[?]", Core.m_recvsize)

local _result = ffinew("poll_result_t *")
local _rw = ffinew("rw_result_t *")

local _intvalue = ffinew("int", 0)
local _int64value = ffinew("int64_t", 0)

function Core.init()
    mnet_core.mnet_init(1)
end

function Core.fini()
    mnet_core.mnet_fini()
end

function Core.report(level)
    mnet_core.mnet_report(level)
end

function Core.current()
   return mnet_current()
end

function Core.poll(milliseconds)
    _result = mnet_poll(milliseconds)
    if _result.chann_count < 0 then
        return -1
    elseif _result.chann_count > 0 then
        local msg = mnet_result_next(_result)
        while msg ~= nil do
            local accept = nil
            if msg.r ~= nil then
                accept = {}
                setmetatable(accept, Chann)
                accept.m_chann = msg.r
                accept.m_type = ChannTypesTable[tonumber(mnet_chann_type(msg.r))]
                AllOpenedChannsTable[tostring(msg.r)] = accept
            end
            local chann = AllOpenedChannsTable[tostring(msg.n)]
            if chann and chann.m_callback then
                chann.m_callback(chann, EventNamesTable[tonumber(msg.event)], accept, msg)
            end
            msg = mnet_result_next(_result)
        end
    end
    return _result.chann_count
end

function Core.resolve(host, port, chann_type)
    local buf = _sendbuf
    if host:len() > Core.m_sendsize then
        buf = ffinew("char[?]", host:len())
    end
    fficopy(buf, host, host:len())
    if mnet_core.mnet_resolve(buf, tonumber(port), _ctype, _addr[0]) > 0 then
        local tbl = {}
        tbl.ip = ffistring(_addr[0].ip, 16)
        tbl.port = tonumber(_addr[0].port)
        return tbl
    else
        return nil
    end
end

function Core.parseIpPort(ipport)
    if mnet_core.mnet_parse_ipport(ipport, _addr[0]) > 0 then
        local tbl = {}
        tbl.ip = ffistring(_addr[0].ip, 16)
        tbl.port = tonumber(_addr[0].port)
        return tbl
    else
        return nil
    end
end

function Core.setBufSize(sendsize, recvsize)
    Core.m_sendsize = math.max(32, sendsize)
    Core.m_recvsize = math.max(32, recvsize)
    _sendbuf = ffinew("uint8_t[?]", Core.m_sendsize)
    _recvbuf = ffinew("uint8_t[?]", Core.m_recvsize)
end

--
-- Channs
--

function Core.openChann(chann_type)
    local chann = {}
    setmetatable(chann, Chann)
    if chann_type == "broadcast" then
        chann.m_chann = mnet_chann_open(mnet_core.CHANN_TYPE_BROADCAST)
    elseif chann_type == "udp" then
        chann.m_chann = mnet_chann_open(mnet_core.CHANN_TYPE_DGRAM)
    else
        chann_type = "tcp"
        chann.m_chann = mnet_chann_open(mnet_core.CHANN_TYPE_STREAM)
    end
    chann.m_type = chann_type
    AllOpenedChannsTable[tostring(chann.m_chann)] = chann
    ffi.gc(chann.m_chann, mnet_chann_close)
    return chann
end

function Core.allChanns()
    local tbl = {}
    for _, v in pairs(AllOpenedChannsTable) do
        if v then
            tbl[#tbl + 1] = v
        end
    end
    return tbl
end

function Chann:close()
    AllOpenedChannsTable[tostring(self.m_chann)] = nil
    mnet_chann_close(self.m_chann)
    self.m_chann = nil
    self.m_callback = nil
    self.m_addr = nil
    self.m_type = nil
end

function Chann:channFd()
    return mnet_chann_fd(self.m_chann)
end

function Chann:channType()
    return self.m_type
end

function Chann:listen(host, port, backlog)
    return mnet_chann_listen(self.m_chann, host, tonumber(port), backlog or 1)
end

function Chann:connect(host, port)
    return mnet_chann_connect(self.m_chann, host, tonumber(port))
end

function Chann:disconnect()
    mnet_chann_disconnect(self.m_chann)
end

-- callback params should be (self, event_name, accept_chann, c_msg)
function Chann:setCallback(callback)
    self.m_callback = callback
end

function Chann:activeEvent(event_name, value)
    if event_name == "event_send" then -- true or false
        _int64value = value and 1 or 0
        mnet_chann_active_event(self.m_chann, mnet_core.CHANN_EVENT_SEND, _int64value)
    elseif event_name == "event_timer" then -- milliseconds
        _int64value = value
        mnet_chann_active_event(self.m_chann, mnet_core.CHANN_EVENT_SEND, _int64value)
    end
end

function Chann:recv()
    _intvalue = Core.m_recvsize
    _rw = mnet_chann_recv(self.m_chann, _recvbuf, _intvalue)
    if _rw.ret <= 0 then
        return nil
    end
    return ffistring(_recvbuf, _rw.ret)
end

function Chann:send(data)
    if type(data) ~= "string" then
        return false
    end
    local leftsize = data:len()
    repeat
        _intvalue = math.min(leftsize, Core.m_sendsize)
        fficopy(_sendbuf, data, _intvalue)
        _rw = mnet_chann_send(self.m_chann, _sendbuf, _intvalue)
        if _rw.ret <= 0 then
            return false
        else
            data = data:sub(_intvalue + 1)
            leftsize = leftsize - _intvalue
        end
    until leftsize <= 0
    return true
end

function Chann:setSocketBufSize(size)
    mnet_core.mnet_chann_set_bufsize(self.m_chann, tonumber(size))
end

function Chann:cachedSize()
    return mnet_chann_cached(self.m_chann)
end

function Chann:addr()
    if not self.m_addr then
        mnet_chann_addr(self.m_chann, _addr[0])
        self.m_addr = {}
        self.m_addr.ip = ffistring(_addr[0].ip, 16)
        self.m_addr.port = tonumber(_addr[0].port)
    end
    return self.m_addr
end

function Chann:state()
    return StateNamesTable[tonumber(mnet_chann_state(self.m_chann)) + 1]
end

function Chann:recvBytes()
    _intvalue = 0
    return tonumber(mnet_chann_bytes(self.m_chann, _intvalue))
end

function Chann:sendByes()
    _intvalue = 1
    return tonumber(mnet_chann_bytes(self.m_chann, _intvalue))
end

return Core
