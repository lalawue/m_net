--
-- Copyright (c) 2019 lalawue
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--

local type = type
local tostring = tostring
local tonumber = tonumber
local pairs = pairs
local setmetatable = setmetatable
local mmin = math.min
local mmax = math.max

local ffi = require "ffi"

ffi.cdef [[
typedef enum {
   CHANN_TYPE_STREAM = 1,
   CHANN_TYPE_DGRAM,
   CHANN_TYPE_BROADCAST,
   CHANN_TYPE_TLS,
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
int mnet_version(void);

/* 0: chann_count
   1: chann_detail
*/
int mnet_report(int level);

/* 0:disable
 * 1:error
 * 2:info
 * 3:verbose
 */
void mnet_setlog(int level, void *);

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

void mnet_chann_set_opaque(chann_t *n, uintptr_t opaque);
uintptr_t mnet_chann_get_opaque(chann_t *n);

void mnet_chann_active_event(chann_t *n, chann_event_t et, int64_t active);

rw_result_t* mnet_chann_recv(chann_t *n, const char *buf, int len);
rw_result_t* mnet_chann_send(chann_t *n, const char *buf, int len);

int mnet_chann_cached(chann_t *n);

int mnet_chann_state(chann_t *n);
long long mnet_chann_bytes(chann_t *n, int be_send);

int mnet_chann_socket_addr(chann_t *n, chann_addr_t*);
int mnet_chann_socket_set_bufsize(chann_t *n, int bufsize);

/* tools without init */
int64_t mnet_current(void); /* micro seconds */
int mnet_resolve(const char *host, int port, chann_type_t ctype, chann_addr_t*);
int mnet_parse_ipport(const char *ipport, chann_addr_t *addr);

int mnet_tls_config(void *ssl_ctx);
]]

-- try to load mnet in package.cpath
local ret, mNet = nil, nil
do
    local loaded = false
    local suffix = (jit.os == "Windows") and "dll" or "so"
    for cpath in package.cpath:gmatch("[^;]+") do
        local path = cpath:sub(1, cpath:len() - 2 - suffix:len()) .. "mnet." .. suffix
        ret, mNet = pcall(ffi.load, path)
        if ret then
            loaded = true
            break
        end
    end
    if not loaded then
        error(mNet)
    end
end

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
    "broadcast",
    "tls",
}

-- mnet core, shared by all channs
local Core = {
    _recvsize = 65536, -- default recv buf size
    _sendsize = 65536, -- default send buf size
    core = mNet -- mnet library
}

-- C level local veriable
local _addr = ffi.new("chann_addr_t[1]")
local _recvbuf = ffi.new("uint8_t[?]", Core._recvsize)

function Core.init()
    mNet.mnet_init(1)
end

function Core.fini()
    mNet.mnet_fini()
end

function Core.setLogLevel(level)
    mNet.mnet_setlog(tonumber(level), nil)
end

function Core.version()
    return mNet.mnet_version()
end

function Core.extConfig(name, ctx)
    if name == "tls" then
        mNet.mnet_tls_config(ctx)
    end
end

function Core.report(level)
    mNet.mnet_report(level)
end

function Core.current()
    return mNet.mnet_current()
end

-- holding chann and callback instance
local Array = {
    _chn = {},
    _free = {},
    _cb = {},
    _count = 0
}

function Array:count()
    return self._count
end

function Array:chnAt(index)
    if index > 0 and index <= #self._chn then
        return self._chn[index]
    end
    return nil
end

function Array:cbAt(index)
    return self._cb[index]
end

function Array:cbUpdate(index, cb)
    if index > 0 then
        self._cb[index] = cb
    end
end

function Array:chnAppend(data)
    local index = 0
    if #self._free > 0 then
        index = table.remove(self._free)
    else
        index = #self._chn + 1
    end
    self._chn[index] = data
    self._cb[index] = ''
    self._count = self._count + 1
    return index
end

function Array:dropIndex(index)
    index = tonumber(index)
    local dcount = #self._chn
    if index <= 0 or index > dcount then
        return nil
    end
    local chn = self._chn[index]
    self._chn[index] = ''
    self._cb[index] = ''
    table.insert(self._free, index)
    self._count = self._count - 1
    return chn
end

-- chann
local Chann = {
    _type = nil, -- 'tcp', 'udp', 'broadcast'
    _chann = nil, -- chann_t
    __tostring = function(t)
        return string.format('<Chann: %p>', t)
    end
}
Chann.__index = Chann

function Core.poll(milliseconds)
    local result = mNet.mnet_poll(milliseconds)
    if result.chann_count < 0 then
        return -1
    elseif result.chann_count > 0 then
        local msg = mNet.mnet_result_next(result)
        while msg ~= nil do
            local accept = nil
            if msg.r ~= nil then
                accept = setmetatable({}, Chann)
                accept._chann = msg.r
                accept._type = ChannTypesTable[tonumber(mNet.mnet_chann_type(msg.r))]
                mNet.mnet_chann_set_opaque(msg.r, Array:chnAppend(accept))
            end
            local index = tonumber(mNet.mnet_chann_get_opaque(msg.n))
            local chann = Array:chnAt(index)
            local callback = Array:cbAt(index)
            if chann and callback then
                callback(chann, EventNamesTable[tonumber(msg.event)], accept, msg)
            end
            msg = mNet.mnet_result_next(result)
        end
    end
    return result.chann_count
end

function Core.resolve(host, port, chann_type)
    local chann_type = ChannTypesTable[chann_type] or mNet.CHANN_TYPE_STREAM
    if mNet.mnet_resolve(host, tonumber(port), chann_type, _addr[0]) > 0 then
        return { ip = ffi.string(_addr[0].ip), port = tonumber(_addr[0].port) }
    end
end

function Core.parseIpPort(ipport)
    if mNet.mnet_parse_ipport(ipport, _addr[0]) > 0 then
        return { ip = ffi.string(_addr[0].ip), port = tonumber(_addr[0].port) }
    end
end

function Core.setBufSize(sendsize, recvsize)
    Core._sendsize = mmax(64, sendsize)
    Core._recvsize = mmax(64, recvsize)
    _recvbuf = ffi.new("uint8_t[?]", Core._recvsize)
end

--
-- Channs
--

function Core.channCount()
    return Array:count()
end

function Core.allChanns()
    local tbl = {}
    for _, v in pairs(Array._chn) do
        if v ~= '' then
            table.insert(tbl, v)
        end
    end
    return tbl
end

function Core.openChann(chann_type)
    local chann = setmetatable({}, Chann)
    if chann_type == "broadcast" then
        chann._chann = mNet.mnet_chann_open(mNet.CHANN_TYPE_BROADCAST)
    elseif chann_type == "udp" then
        chann._chann = mNet.mnet_chann_open(mNet.CHANN_TYPE_DGRAM)
    elseif chann_type == "tls" then
        chann_type = "tls"
        chann._chann = mNet.mnet_chann_open(mNet.CHANN_TYPE_TLS)
    else
        chann_type = "tcp"
        chann._chann = mNet.mnet_chann_open(mNet.CHANN_TYPE_STREAM)
    end
    chann._type = chann_type
    mNet.mnet_chann_set_opaque(chann._chann, Array:chnAppend(chann))
    return chann
end

function Chann:close()
    if self._chann ~= nil then
        Array:dropIndex(mNet.mnet_chann_get_opaque(self._chann))
        mNet.mnet_chann_set_opaque(self._chann, 0)
        mNet.mnet_chann_close(self._chann)
        self._chann = nil
        self._type = nil
        setmetatable(self, nil)
    end
end

function Chann:channFd()
    return mNet.mnet_chann_fd(self._chann)
end

function Chann:channType()
    return self._type
end

function Chann:listen(host, port, backlog)
    return mNet.mnet_chann_listen(self._chann, host, tonumber(port), backlog or 1)
end

function Chann:connect(host, port)
    return mNet.mnet_chann_connect(self._chann, host, tonumber(port))
end

function Chann:disconnect()
    mNet.mnet_chann_disconnect(self._chann)
end

-- callback params should be (self, event_name, accept_chann, c_msg)
function Chann:setCallback(callback)
    if self._chann then
        local index = tonumber(mNet.mnet_chann_get_opaque(self._chann))
        Array:cbUpdate(index, callback)
    end
end

function Chann:activeEvent(event_name, value)
    if event_name == "event_send" then -- true or false
        mNet.mnet_chann_active_event(self._chann, mNet.CHANN_EVENT_SEND, value and 1 or 0)
    elseif event_name == "event_timer" then -- milliseconds
        mNet.mnet_chann_active_event(self._chann, mNet.CHANN_EVENT_SEND, value)
    end
end

function Chann:recv()
    local len = Core._recvsize
    local rw = mNet.mnet_chann_recv(self._chann, _recvbuf, len)
    if rw.ret <= 0 then
        return nil
    end
    return ffi.string(_recvbuf, rw.ret)
end

function Chann:send(data)
    if type(data) ~= "string" then
        return false
    end
    local rw = mNet.mnet_chann_send(self._chann, data, data:len())
    if rw.ret <= 0 then
        return false
    else
        return true
    end
end

function Chann:cachedSize()
    return mNet.mnet_chann_cached(self._chann)
end

function Chann:state()
    return StateNamesTable[tonumber(mNet.mnet_chann_state(self._chann)) + 1]
end

function Chann:recvBytes()
    return tonumber(mNet.mnet_chann_bytes(self._chann, 0))
end

function Chann:sendByes()
    return tonumber(mNet.mnet_chann_bytes(self._chann, 1))
end

function Chann:setBufSize(size)
    mNet.mnet_chann_socket_set_bufsize(self._chann, tonumber(size))
end

function Chann:addr()
    if self:state() == "state_connected" then
        mNet.mnet_chann_socket_addr(self._chann, _addr[0])
        return {ip = ffi.string(_addr[0].ip), port = tonumber(_addr[0].port)}
    end
    return nil
end

return Core
