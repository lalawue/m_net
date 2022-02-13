--
-- Copyright (c) 2020 lalawue
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--

--[[
    A DNS query tool build on top of MNet's event loop
]]

local ffi = require("ffi")

ffi.cdef([[
/* -- Build query content
 * buf: buffer for store DNS query packet content, must have size == 1024 bytes
 * qid: DNS query id, make sure > 0
 * domain: '0' terminated string
 * --
 * return: 0 for error, or valid query_size > 0
 */
 int mdns_query_build(uint8_t *buf, unsigned short qid, const char *domain);

 /* -- fetch query id for later input query_size
  * buf: response buffer, must have size == 1024 byte
  * content_len: valid content length
  * --
  * return: 0 for error, or qid from response
  */
 int mdns_response_fetch_qid(const uint8_t *buf, int content_len);
 
 /* -- 
  * buf: response buffer, must have size == 1024 byte
  * content_len: valid content length
  * query_size: query_size for qid fetched before
  * domain: '0' terminated string for compare
  * out_ipv4: 4 byte buffer for output ipv4
  * --
  * return: 0 for error, 1 for ok
  */
 int mdns_response_parse(uint8_t *buf,
                         int content_len,
                         int query_size,
                         const char *domain,
                         uint8_t *out_ipv4);
]])

local NetCore = require("ffi-mnet")
local mNet = NetCore.core

local _copy = ffi.copy
local _fill = ffi.fill
local _string = ffi.string

local _buf = ffi.new("uint8_t[?]", 1024)
local _domain = ffi.new("uint8_t[?]", 256)
local _out_ipv4 = ffi.new("uint8_t[?]", 4)

-- build UDP package
local function _sendRequest(self, domain, callback)
    local qid = math.random(65535)
    _fill(_domain, 256)
    _copy(_domain, domain)
    local query_size = mNet.mdns_query_build(_buf, qid, _domain)
    if query_size <= 0 then
        return false
    end
    local item = { domain = domain, query_size = query_size }
    self._qid_tbl[qid] = item
    local data = _string(_buf, query_size)
    for _, chann in ipairs(self._svr_tbl) do
        chann:send(data)
    end
    -- add response to wait list
    if not self._wait_tbl[domain] then
        self._wait_tbl[domain] = {}
    end
    local tbl = self._wait_tbl[domain]
    tbl[#tbl + 1] = callback
    return true
end

-- got domain response
local function _rpcResponse(self, domain, data)
    local tbl = self._wait_tbl[domain]
    if tbl then
        for _, callback in ipairs(tbl) do
            callback(data)
        end
        self._wait_tbl[domain] = nil
    end
end

-- recv UDP data
local function _recvResponse(self, pkg_data)
    if pkg_data == nil then
        return
    end
    -- check response
    _copy(_buf, pkg_data, pkg_data:len())
    local qid = tonumber(mNet.mdns_response_fetch_qid(_buf, pkg_data:len()))
    local item = self._qid_tbl[qid]
    if item == nil then
        return
    end
    _fill(_domain, 256)
    _copy(_domain, item.domain)
    local ret = mNet.mdns_response_parse(_buf, pkg_data:len(), item.query_size, _domain, _out_ipv4)
    if ret <= 0 then
        _rpcResponse(self, item.domain, nil)
    else
        local out = _string(_out_ipv4, 4)
        local ipv4 = string.format("%d.%d.%d.%d", out:byte(1), out:byte(2), out:byte(3), out:byte(4))
        self._domain_tbl[item.domain] = ipv4
        -- process wait response
        _rpcResponse(self, item.domain, ipv4)
    end
    -- remove from wait processing list
    self._qid_tbl[qid] = nil
end

-- 
--

local _M = {    
}
_M.__index = _M

function _M:init()
    NetCore.init()
    math.randomseed(os.time())
    self._svr_tbl = {} -- UDP chann
    self._qid_tbl = {} -- processing list as [tonumber(qid)] = { domain, query_size }
    self._domain_tbl = {} -- ["domain"] = "ipv4"
    self._wait_tbl = {} -- waiting response
end

-- UDP DNS Query
function _M:initUdpChanns(ipv4_list)
    local dns_ipv4 = {
        "114.114.114.114",
        "8.8.8.8"
    }
    dns_ipv4 = ipv4_list or dns_ipv4
    for i = 1, #dns_ipv4, 1 do
        local udp_chann = NetCore.openChann("udp")
        udp_chann:setCallback(function(chann, event_name, _, _)
            if event_name == "event_recv" then
                _recvResponse(self, chann:recv())
            end
        end)
        udp_chann:connect(dns_ipv4[i], 53)
        self._svr_tbl[#self._svr_tbl + 1] = udp_chann
    end
end

-- init after mnet init
local function _init(ipv4_list)
    _M:init()
    _M:initUdpChanns(ipv4_list)
end

-- query domain, return ipv4 in coroutine
local function _queryHost(domain)
    if type(domain) ~= "string" then
        return false
    end
    local ipv4 = _M._domain_tbl[domain]
    if ipv4 then
        return ipv4
    else
        local co = coroutine.running()
        local ret_func = function(ipv4)
            coroutine.resume(co, ipv4)
        end
        local ret = _sendRequest(_M, domain, ret_func)
        if not ret then
            return false
        end
        return coroutine.yield(ret)
    end
end

return {
    init = _init,
    queryHost = _queryHost
}
