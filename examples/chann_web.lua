--
-- Copyright (c) 2019 lalawue
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--

local Core = nil
local ipport = ...

if jit then
    Core = require("ffi-mnet")
    print("using jit", _VERSION)
else
    Core = require("mnet-chann")
    print("using", _VERSION)
end

Core.init()

ipport = ipport or "127.0.0.1:8080"
local addr = Core.parseIpPort(ipport)
print("using version", Core.version())
print("open svr in " .. addr.ip .. ":" .. addr.port)

-- open tcp stream
local svr = Core.openChann("tcp")
svr:listen(addr.ip, addr.port, 100)

local http_response = ''
do
    local fp = io.open('README.md', 'rb')
    if fp then
        local content = fp:read("*a")
        fp:close()
        http_response = 'HTTP/1.1 200 OK\r\n' ..
                        'Content-Type: text/plain\r\n' ..
                        'Content-Length: ' .. content:len() .. '\r\n\r\n' ..
                        content
    end
end

local recv_data = ''

-- client callback function
local function clientCallback(self, eventName, accept, c_msg)
    --print("eventName: ", eventName)
    if eventName == "event_recv" then
        local buf = self:recv()
        if buf:len() > 0 then
            recv_data = recv_data .. buf
        end
        local last_pos = 1
        while true do
            local s, e = recv_data:find('\r\n\r\n', last_pos, true)
            if s and e then
                last_pos = e + 1
                self:send(http_response)
            else
                recv_data = recv_data:sub(last_pos)
                break
            end
        end
    elseif eventName == "event_disconnect" then
        self:close()
    end
end

-- server callback function
svr:setCallback(
    function(self, eventName, accept)
        if eventName == "event_accept" and accept then
            accept:setCallback(clientCallback) -- client callback function
        end
    end
)

while Core.poll(1000) do
    -- do nothing
end

Core.fini()
