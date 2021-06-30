
--
-- Copyright (c) 2021 lalawue
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--

--
-- Usage: luajit test_mdns.lua 'www.github.com' 'www.sina.com'

if not jit then
    print("Please use LuaJIT !")
    os.exit(0)
end

local NetCore = require("ffi-mnet")
local mDNS = require("ffi-mdns")
print("using " .. jit.version)
print("---")

mDNS.init()

local wait_count = 0
local wait_list = {
    "www.baidu.com"
}
for _, v in ipairs(arg) do
    wait_list[#wait_list + 1] = v
end

local co = coroutine.create(function()
    while true do
        wait_count = wait_count + 1
        local domain = wait_list[wait_count]
        if domain then
            local ipv4 = mDNS.queryHost(domain)
            print("query", domain, ipv4)
        else
            break
        end
    end
end)
coroutine.resume(co)

while true do
    if wait_count > #wait_list then
        break
    end
    NetCore.poll(100000)
end