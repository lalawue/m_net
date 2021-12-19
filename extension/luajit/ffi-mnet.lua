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

-- try to load mnet in package.cpath
local ret, mNet = nil, nil
do
    local suffix = (jit.os == "Windows") and "dll" or "so"
    for cpath in package.cpath:gmatch("[^;]+") do
        local path = cpath:sub(1, cpath:len() - 2 - suffix:len()) .. "mnet." .. suffix
        ret, mNet = pcall(ffi.load, path)
        if ret then
            goto SUCCESS_LOAD_LABEL
        end
    end
    error(mNet)
    ::SUCCESS_LOAD_LABEL::
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
    "broadcast"
}

local AllOpenedChannsTable = setmetatable({}, {__mode = "k"}) -- all opened channs

-- mnet core, shared by all channs
local Core = {
    _recvsize = 65536, -- default recv buf size
    _sendsize = 65536, -- default send buf size
    core = mNet -- mnet library
}

-- C level local veriable
local _addr = ffi.new("chann_addr_t[1]")
local _ctype = ffi.new("chann_type_t", 0)

local _sendbuf = ffi.new("uint8_t[?]", Core._sendsize)
local _recvbuf = ffi.new("uint8_t[?]", Core._recvsize)

local _result = ffi.new("poll_result_t *")
local _rw = ffi.new("rw_result_t *")

local _intvalue = ffi.new("int", 0)
local _int64value = ffi.new("int64_t", 0)

function Core.init()
    mNet.mnet_init(1)
end

function Core.fini()
    mNet.mnet_fini()
end

function Core.report(level)
    mNet.mnet_report(level)
end

function Core.current()
    return mNet.mnet_current()
end

local function _gcChannInstance(c_chann)
    local chann = AllOpenedChannsTable[tostring(c_chann)]
    if chann then
        chann:close()
    end
end

-- chann
local Chann = {
    _type = nil, -- 'tcp', 'udp', 'broadcast'
    _chann = nil, -- chann_t
    _callback = nil -- callback
}
Chann.__index = Chann

function Core.poll(milliseconds)
    _result = mNet.mnet_poll(milliseconds)
    if _result.chann_count < 0 then
        return -1
    elseif _result.chann_count > 0 then
        local msg = mNet.mnet_result_next(_result)
        while msg ~= nil do
            local accept = nil
            if msg.r ~= nil then
                accept = {}
                setmetatable(accept, Chann)
                accept._chann = msg.r
                accept._type = ChannTypesTable[tonumber(mNet.mnet_chann_type(msg.r))]
                AllOpenedChannsTable[tostring(msg.r)] = accept
                ffi.gc(accept._chann, _gcChannInstance)
            end
            local chann = AllOpenedChannsTable[tostring(msg.n)]
            if chann and chann._callback then
                chann._callback(chann, EventNamesTable[tonumber(msg.event)], accept, msg)
            end
            msg = mNet.mnet_result_next(_result)
        end
    end
    return _result.chann_count
end

function Core.resolve(host, port, chann_type)
    local buf = _sendbuf
    if host:len() > Core._sendsize then
        buf = ffi.new("char[?]", host:len())
    end
    ffi.copy(buf, host, host:len())
    if mNet.mnet_resolve(buf, tonumber(port), _ctype, _addr[0]) > 0 then
        local tbl = {}
        tbl.ip = ffi.string(_addr[0].ip)
        tbl.port = tonumber(_addr[0].port)
        return tbl
    else
        return nil
    end
end

function Core.parseIpPort(ipport)
    if mNet.mnet_parse_ipport(ipport, _addr[0]) > 0 then
        local tbl = {}
        tbl.ip = ffi.string(_addr[0].ip)
        tbl.port = tonumber(_addr[0].port)
        return tbl
    else
        return nil
    end
end

function Core.setBufSize(sendsize, recvsize)
    Core._sendsize = mmax(32, sendsize)
    Core._recvsize = mmax(32, recvsize)
    _sendbuf = ffi.new("uint8_t[?]", Core._sendsize)
    _recvbuf = ffi.new("uint8_t[?]", Core._recvsize)
end

--
-- Channs
--

function Core.openChann(chann_type)
    local chann = {}
    setmetatable(chann, Chann)
    if chann_type == "broadcast" then
        chann._chann = mNet.mnet_chann_open(mNet.CHANN_TYPE_BROADCAST)
    elseif chann_type == "udp" then
        chann._chann = mNet.mnet_chann_open(mNet.CHANN_TYPE_DGRAM)
    else
        chann_type = "tcp"
        chann._chann = mNet.mnet_chann_open(mNet.CHANN_TYPE_STREAM)
    end
    chann._type = chann_type
    AllOpenedChannsTable[tostring(chann._chann)] = chann
    ffi.gc(chann._chann, _gcChannInstance)
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
    if self._chann then
        ffi.gc(self._chann, nil)
        AllOpenedChannsTable[tostring(self._chann)] = nil
        mNet.mnet_chann_close(self._chann)
        self._chann = nil
        self._callback = nil
        self._type = nil
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
    self._callback = callback
end

function Chann:activeEvent(event_name, value)
    if event_name == "event_send" then -- true or false
        _int64value = value and 1 or 0
        mNet.mnet_chann_active_event(self._chann, mNet.CHANN_EVENT_SEND, _int64value)
    elseif event_name == "event_timer" then -- milliseconds
        _int64value = tonumber(value)
        mNet.mnet_chann_active_event(self._chann, mNet.CHANN_EVENT_SEND, _int64value)
    end
end

function Chann:recv()
    _intvalue = Core._recvsize
    _rw = mNet.mnet_chann_recv(self._chann, _recvbuf, _intvalue)
    if _rw.ret <= 0 then
        return nil
    end
    return ffi.string(_recvbuf, _rw.ret)
end

function Chann:send(data)
    if type(data) ~= "string" then
        return false
    end
    local leftsize = data:len()
    repeat
        _intvalue = mmin(leftsize, Core._sendsize)
        ffi.copy(_sendbuf, data, _intvalue)
        _rw = mNet.mnet_chann_send(self._chann, _sendbuf, _intvalue)
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
    mNet.mnet_chann_set_bufsize(self._chann, tonumber(size))
end

function Chann:cachedSize()
    return mNet.mnet_chann_cached(self._chann)
end

function Chann:addr()
    if self:state() == "state_connected" then
        mNet.mnet_chann_addr(self._chann, _addr[0])
        return {ip = ffi.string(_addr[0].ip), port = tonumber(_addr[0].port)}
    end
    return nil
end

function Chann:state()
    return StateNamesTable[tonumber(mNet.mnet_chann_state(self._chann)) + 1]
end

function Chann:recvBytes()
    _intvalue = 0
    return tonumber(mNet.mnet_chann_bytes(self._chann, _intvalue))
end

function Chann:sendByes()
    _intvalue = 1
    return tonumber(mNet.mnet_chann_bytes(self._chann, _intvalue))
end

return Core
