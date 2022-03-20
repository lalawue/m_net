--
-- Copyright (c) 2022 lalawue
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--

local ffi = require "ffi"

ffi.cdef [[
    enum {
        SSL_VERIFY_NONE = 0x00,
        SSL_VERIFY_PEER = 0x01,
        SSL_VERIFY_FAIL_IF_NO_PEER_CERT = 0x02,
        SSL_VERIFY_CLIENT_ONCE = 0x04,
        SSL_VERIFY_POST_HANDSHAKE = 0x08,
    };
    enum {
        SSL_FILETYPE_PEM = 1,
        SSL_FILETYPE_ASN1 = 2,
    };
    void OPENSSL_init_ssl(uint64_t, void*);
    void* TLS_method(void);
    void* SSL_CTX_new(void *);
    void SSL_CTX_set_verify(void*, int, void*);
    int SSL_CTX_load_verify_locations(void*, const char*, const char*);
    int SSL_CTX_use_certificate_file(void*, const char*, int);
    int SSL_CTX_use_PrivateKey_file(void*, const char *, int);
    int SSL_CTX_check_private_key(void*);
]]

local Core = nil
local ipport = ...

if jit then
    Core = require("ffi-mnet")
    print("### using jit", _VERSION)
else
    print("### only support ffi-mnet with OpenSSL buildin")
    os.exit(0)
end

Core.init()

ipport = ipport or "127.0.0.1:8080"
local addr = Core.parseIpPort(ipport)
print("### open svr in " .. addr.ip .. ":" .. addr.port)

-- default 256, 256
Core.setBufSize(32, 1024)

-- init tls
do
    local openssl = Core.core
    openssl.OPENSSL_init_ssl(0, nil)
    local ctx = openssl.SSL_CTX_new(openssl.TLS_method())
    openssl.SSL_CTX_set_verify(ctx, openssl.SSL_VERIFY_PEER, nil)
    openssl.SSL_CTX_load_verify_locations(ctx, "examples/openssl/ca.crt", nil)
    openssl.SSL_CTX_use_certificate_file(ctx, "examples/openssl/server.crt", openssl.SSL_FILETYPE_PEM)
    openssl.SSL_CTX_use_PrivateKey_file(ctx, "examples/openssl/server.key", openssl.SSL_FILETYPE_PEM);
    assert(openssl.SSL_CTX_check_private_key(ctx) == 1)
    Core.extConfig("tls", ctx)
    print("### config TLS context")
end

-- open tcp stream
local svr = Core.openChann("tls")
svr:listen(addr.ip, addr.port, 100)

-- client callback function
local function clientCallback(self, eventName, accept, c_msg)
    --print("eventName: ", eventName)
    if eventName == "event_recv" then
        local buf = self:recv()
        print("### recv:")
        print(buf)
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
        print("### client ip: " .. addr.ip .. ":" .. addr.port)
        print("### state: " .. self:state())
        print("### bytes send: " .. self:sendByes())
        print("---")
        self:close()
    end
end

-- server callback function
svr:setCallback(
    function(self, eventName, accept)
        if eventName == "event_accept" and accept then
            local addr = accept:addr()
            print("### accept client from " .. addr.ip .. ":" .. addr.port)
            accept:setCallback(clientCallback) -- client callback function
        end
    end
)

while Core.poll(1000) do
    -- do nothing
end

Core.fini()
