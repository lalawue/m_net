--
-- Copyright (c) 2017 lalawue
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--

local type = type
local tostring = tostring
local tonumber = tonumber
local pairs = pairs
local setmetatable = setmetatable

local mnet_core = require("mnet")

local _poll = mnet_core.poll
local _result_next = mnet_core.result_next
local _chann_open = mnet_core.chann_open
local _chann_close = mnet_core.chann_close
local _chann_fd = mnet_core.chann_fd
local _chann_type = mnet_core.chann_type
local _chann_listen = mnet_core.chann_listen
local _chann_connect = mnet_core.chann_connect
local _chann_disconnect = mnet_core.chann_disconnect
local _chann_set_index = mnet_core.chann_set_index
local _chann_get_index = mnet_core.chann_get_index
local _chann_active_event = mnet_core.chann_active_event
local _chann_recv = mnet_core.chann_recv
local _chann_send = mnet_core.chann_send
local _chann_cached = mnet_core.chann_cached
local _chann_state = mnet_core.chann_state
local _chann_bytes = mnet_core.chann_bytes

local _chann_set_bufsize = mnet_core.chann_socket_set_bufsize
local _chann_addr = mnet_core.chann_socket_addr

local _current = mnet_core.current
local _resolve = mnet_core.resolve
local _parse_ipport = mnet_core.parse_ipport

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
    "tls"
}

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

-- mnet core, shared by all channs
local Core = {
    CHANN_TYPE_STREAM = 1,
    CHANN_TYPE_DGRAM = 2,
    CHANN_TYPE_BROADCAST = 3,
    CHANN_TYPE_TLS = 4,
    --
    CHANN_EVENT_RECV = 1,
    CHANN_EVENT_SEND = 2,
    CHANN_EVENT_ACCEPT = 3,
    CHANN_EVENT_CONNECTED = 4,
    CHANN_EVENT_DISCONNECT = 5,
    CHANN_EVENT_TIMER = 6
}

function Core.init()
    mnet_core.init()
end

function Core.fini()
    mnet_core.fini()
end

function Core.version()
    return mnet_core.version()
end

function Core.current()
    return _current()
end

function Core.poll(milliseconds)
    local chann_count = _poll(milliseconds)
    if chann_count < 0 then
        return -1
    elseif chann_count > 0 then
        while true do
            local n, event, r = _result_next()
            if n == nil then
                break
            end
            local accept = nil
            if r ~= nil then
                accept = setmetatable({}, Chann)
                accept._chann = r
                accept._type = ChannTypesTable[_chann_type(r)]
                _chann_set_index(r, Array:chnAppend(accept))
            end
            local index = _chann_get_index(n)
            local chann = Array:chnAt(index)
            local callback = Array:cbAt(index)
            if chann and callback then
                callback(chann, EventNamesTable[tonumber(event)], accept)
            end
        end
    end
    return chann_count
end

function Core.resolve(host, port, chann_type)
    local ip, port = _resolve(host, port, ChannTypesTable[chann_type] or 1)
    if ip and port then
        return { ip = ip, port = port }
    end
end

function Core.parseIpPort(ipport)
    local ip, port = _parse_ipport(ipport)
    if ip and port then
        return { ip = ip, port = port }
    end
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
    local chann = {}
    setmetatable(chann, Chann)
    if chann_type == "broadcast" then
        chann._chann = _chann_open(Core.CHANN_TYPE_BROADCAST)
    elseif chann_type == "udp" then
        chann._chann = _chann_open(Core.CHANN_TYPE_DGRAM)
    elseif chann_type == "tls" then
        chann._chann = _chann_open(Core.CHANN_TYPE_TLS)
    else
        chann_type = "tcp"
        chann._chann = _chann_open(Core.CHANN_TYPE_STREAM)
    end
    chann._type = chann_type
    _chann_set_index(chann._chann, Array:chnAppend(chann))
    return chann
end

function Chann:close()
    if self._chann then
        Array:dropIndex(_chann_get_index(self._chann))
        _chann_close(self._chann)
        self._chann = nil
        self._type = nil
        setmetatable(self, nil)
    end
end

function Chann:channFd()
    return _chann_fd(self._chann)
end

function Chann:channType()
    return self._type
end

function Chann:listen(host, port, backlog)
    return _chann_listen(self._chann, host, tonumber(port), backlog or 1)
end

function Chann:connect(host, port)
    return _chann_connect(self._chann, host, tonumber(port))
end

function Chann:disconnect()
    _chann_disconnect(self._chann)
end

-- callback params should be (self, event_name, accept_chann, c_msg)
function Chann:setCallback(callback)
    if self._chann then
        Array:cbUpdate(_chann_get_index(self._chann), callback)
    end
end

function Chann:activeEvent(event_name, value)
    if event_name == "event_send" then -- true or false
        _chann_active_event(self._chann, Core.CHANN_EVENT_SEND, value and 1 or 0)
    elseif event_name == "event_timer" then -- milliseconds
        _chann_active_event(self._chann, Core.CHANN_EVENT_TIMER, tonumber(value))
    end
end

function Chann:recv()
    return _chann_recv(self._chann)
end

function Chann:send(data)
    if type(data) ~= "string" then
        return false
    end
    _chann_send(self._chann, data)
    return true
end

function Chann:cachedSize()
    return _chann_cached(self._chann)
end

function Chann:state()
    return StateNamesTable[tonumber(_chann_state(self._chann)) + 1]
end

function Chann:recvBytes()
    return tonumber(_chann_bytes(self._chann, 0))
end

function Chann:sendByes()
    return tonumber(_chann_bytes(self._chann, 1))
end

function Chann:setSocketBufSize(size)
    _chann_set_bufsize(self._chann, tonumber(size))
end

function Chann:addr()
    if self:state() == "state_connected" then
        local ip, port = _chann_addr(self._chann)
        return {ip = ip, port = tonumber(port)}
    end
    return nil
end

return Core
