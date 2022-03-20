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
local host = ...

if jit then
    Core = require("ffi-mnet")
    print("### using jit", _VERSION)
else
    print("### only support ffi-mnet with OpenSSL buildin")
    os.exit(0)
end

Core.init()

if not host or host:find("https://") ~= 1 then
    print("Usage: ./examples/openssl/tls_web_cnt.lua https://www.baidu.com")
    os.exit(0)
end

local addr = Core.resolve(host:sub(9), 443)
print("### try connect " .. host .. " as " .. addr.ip .. ":" .. addr.port)

-- init tls
do
    local openssl = Core.core
    openssl.OPENSSL_init_ssl(0, nil)
    local ctx = openssl.SSL_CTX_new(openssl.TLS_method())
    openssl.SSL_CTX_set_verify(ctx, openssl.SSL_VERIFY_NONE, nil)
    --openssl.SSL_CTX_load_verify_locations(ctx, "examples/openssl/ca.crt", nil)
    --openssl.SSL_CTX_use_certificate_file(ctx, "examples/openssl/server.crt", openssl.SSL_FILETYPE_PEM)
    --openssl.SSL_CTX_use_PrivateKey_file(ctx, "examples/openssl/server.key", openssl.SSL_FILETYPE_PEM);
    --assert(openssl.SSL_CTX_check_private_key(ctx) == 1)
    Core.extConfig("tls", ctx)
    print("### config TLS context")
end

-- open tcp stream
local cnt = Core.openChann("tls")

-- cnt callback function
cnt:setCallback(
    function(self, eventName)
        if eventName == "event_connected" then
            print("### connected server")
            local request = "GET / HTTP/1.1\r\n" ..
                            "HOST: " .. host:sub(9) .. "\r\n" ..
                            "User-Agent: MNet TLS Client Example\r\n" ..
                            "Accept: text/html\r\n" ..
                            "Content-Length: 0\r\n\r\n"
            self:send(request)
            print("### send request length: ", request:len())
        elseif eventName == "event_recv" then
            local data = self:recv()
            print("### recv data:")
            print("----------")
            print(data)
            print("----------")
            print("### close socket")
            self:close()
        elseif eventName == "event_disconnect" then
            print("disconnect server")
            self:close()
        end
    end
)

cnt:connect(addr.ip, addr.port)

while Core.poll(1000) > 0 do
    -- do nothing
end

Core.fini()
print("### bye")
