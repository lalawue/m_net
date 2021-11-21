--
-- Copyright (c) 2017 lalawue
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--

type = type
tostring = tostring
tonumber = tonumber
pairs = pairs
setmetatable = setmetatable

local mnet_core = require("mnet")

local mnet_poll = mnet_core.poll
local mnet_result_count = mnet_core.result_count
local mnet_result_next = mnet_core.result_next
local mnet_chann_open = mnet_core.chann_open
local mnet_chann_close = mnet_core.chann_close
local mnet_chann_fd = mnet_core.chann_fd
local mnet_chann_type = mnet_core.chann_type
local mnet_chann_listen = mnet_core.chann_listen
local mnet_chann_connect = mnet_core.chann_connect
local mnet_chann_disconnect = mnet_core.chann_disconnect
local mnet_chann_active_event = mnet_core.chann_active_event
local mnet_chann_recv = mnet_core.chann_recv
local mnet_chann_send = mnet_core.chann_send
local mnet_chann_cached = mnet_core.chann_cached
local mnet_chann_addr = mnet_core.chann_addr
local mnet_chann_state = mnet_core.chann_state
local mnet_chann_bytes = mnet_core.chann_bytes
--local mnet_chann_set_bufsize = mnet_core.chann_set_bufsize
local mnet_current = mnet_core.current
local mnet_parse_ipport = mnet_core.parse_ipport

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
    _type = nil, -- 'tcp', 'udp', 'broadcast'
    _chann = nil, -- chann_t
    _callback = nil -- callback
}
Chann.__index = Chann

-- mnet core, shared by all channs
local Core = {
    CHANN_TYPE_STREAM = 1,
    CHANN_TYPE_DGRAM = 2,
    CHANN_TYPE_BROADCAST = 3,
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

function Core.report(level)
    -- not implement
end

function Core.current()
    return mnet_current()
end

function Core.poll(milliseconds)
    local result = mnet_poll(milliseconds)
    local chann_count = mnet_result_count(result)
    if chann_count < 0 then
        return -1
    elseif chann_count > 0 then
        while true do
            local n, event, r = mnet_result_next(result)
            if n == nil then
                break
            end
            local accept = nil
            if r ~= nil then
                accept = {}
                setmetatable(accept, Chann)
                accept._chann = r
                accept._type = ChannTypesTable[tonumber(mnet_chann_type(r))]
                AllOpenedChannsTable[tostring(r)] = accept
            end
            local chann = AllOpenedChannsTable[tostring(n)]
            if chann and chann._callback then
                chann._callback(chann, EventNamesTable[tonumber(event)], accept)
            end
        end
    end
    return chann_count
end

function Core.resolve(host, port, chann_type)
    -- not implement
end

function Core.parseIpPort(ipport)
    local ip, port = mnet_parse_ipport(ipport)
    if ip and port then
        return {["ip"] = ip, ["port"] = tonumber(port)}
    end
end

function Core.setBufSize(sendsize, recvsize)
end

--
-- Channs
--

function Core.openChann(chann_type)
    local chann = {}
    setmetatable(chann, Chann)
    if chann_type == "broadcast" then
        chann._chann = mnet_chann_open(Core.CHANN_TYPE_BROADCAST)
    elseif chann_type == "udp" then
        chann._chann = mnet_chann_open(Core.CHANN_TYPE_DGRAM)
    else
        chann_type = "tcp"
        chann._chann = mnet_chann_open(Core.CHANN_TYPE_STREAM)
    end
    chann._type = chann_type
    AllOpenedChannsTable[tostring(chann._chann)] = chann
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
        AllOpenedChannsTable[tostring(self._chann)] = nil
        mnet_chann_close(self._chann)
        self._chann = nil
        self._callback = nil
        self._type = nil
    end
end

function Chann:channFd()
    return mnet_chann_fd(self._chann)
end

function Chann:channType()
    return self._type
end

function Chann:listen(host, port, backlog)
    return mnet_chann_listen(self._chann, host, tonumber(port), backlog or 1)
end

function Chann:connect(host, port)
    return mnet_chann_connect(self._chann, host, tonumber(port))
end

function Chann:disconnect()
    mnet_chann_disconnect(self._chann)
end

-- callback params should be (self, event_name, accept_chann, c_msg)
function Chann:setCallback(callback)
    self._callback = callback
end

function Chann:activeEvent(event_name, value)
    if event_name == "event_send" then -- true or false
        mnet_chann_active_event(self._chann, Core.CHANN_EVENT_SEND, value and 1 or 0)
    elseif event_name == "event_timer" then -- milliseconds
        mnet_chann_active_event(self._chann, Core.CHANN_EVENT_TIMER, tonumber(value))
    end
end

function Chann:recv()
    return mnet_chann_recv(self._chann)
end

function Chann:send(data)
    if type(data) ~= "string" then
        return false
    end
    mnet_chann_send(self._chann, data)
    return true
end

function Chann:setSocketBufSize(size)
    mnet_core.chann_set_bufsize(self._chann, tonumber(size))
end

function Chann:cachedSize()
    return mnet_chann_cached(self._chann)
end

function Chann:addr()
    if self:state() == "state_connected" then
        local ip, port = mnet_chann_addr(self._chann)
        return {ip = ip, port = tonumber(port)}
    end
    return nil
end

function Chann:state()
    return StateNamesTable[tonumber(mnet_chann_state(self._chann)) + 1]
end

function Chann:recvBytes()
    return tonumber(mnet_chann_bytes(self._chann, 0))
end

function Chann:sendByes()
    return tonumber(mnet_chann_bytes(self._chann, 1))
end

return Core
