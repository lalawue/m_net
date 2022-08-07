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
print("open svr in " .. addr.ip .. ":" .. addr.port)

-- open tcp stream
local svr = Core.openChann("tcp")
svr:listen(addr.ip, addr.port, 100)

-- client callback function
local function clientCallback(self, eventName, accept, c_msg)
    --print("eventName: ", eventName)
    if eventName == "event_recv" then
        local buf = self:recv()
        print("recv:\n", buf)

        local toast = "hello, world !\n"

        local data =
            "HTTP/1.1 200 OK\r\n" ..
            "Server: MnetChannWeb/0.1\r\n" ..
                "Content-Type: text/plain\r\n" ..
                    string.format("Content-Length: %d\r\n\r\n", toast:len()) .. toast .. "\n\n"

        -- receive 'event_send' when send buffer was empty
        self:activeEvent("event_send", true)
        self:send(data)
    elseif eventName == "event_disconnect" or eventName == "event_send" then
        local addr = self:addr()
        print("---")
        print("client ip: " .. addr.ip .. ":" .. addr.port)
        print("state: " .. self:state())
        print("bytes send: " .. self:sendByes())
        self:close()
    end
end

-- server callback function
svr:setCallback(
    function(self, eventName, accept)
        if eventName == "event_accept" and accept then
            local addr = accept:addr()
            print("---")
            print("accept client from " .. addr.ip .. ":" .. addr.port)
            accept:setCallback(clientCallback) -- client callback function
        end
    end
)

while Core.poll(1000) do
    -- do nothing
end

Core.fini()
