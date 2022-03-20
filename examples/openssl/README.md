

- [Build](#build)
- [Run](#run)
  - [Server](#server)
  - [Client](#client)
- [LuaJIT wrapper](#luajit-wrapper)
  - [Server](#server-1)
  - [Client](#client-1)
- [Test](#test)
  - [reconnect](#reconnect)
  - [rwdata](#rwdata)
- [Usage](#usage)


# Build

First export MNET_OPENSSL_DIR to openssl library dir, here is example for I `brew install openssl` under MacOS, then `make openssl`

```
$ export MNET_OPENSSL_DIR=/usr/local/Cellar/openssl@1.1/1.1.1k/
$ make openssl
```

# Run

Config `LD_LIBRARY_PATH` first, for example

```
$ exoprt LD_LIBARY_PATH=/usr/local/Cellar/openssl@1.1/1.1.1k/
$ export DY_LD_LIBARY_PATH=/usr/local/Cellar/openssl@1.1/1.1.1k/
```

## Server

run server waiting for client request

```
$ ./build/tls_svr
svr start listen: 127.0.0.1:8080
svr accept cnt with chann 127.0.0.1:63887
svr recv resquest: 57
---
GET / HTTP/1.1
User-Agent: MNet/OpenSSL
Accept: */*


---
svr send response 88
---
HTTP/1.1 200 OK
Content-Type: text/plain
Content-Length: 18

Hello MNet/OpenSSL


---
```

## Client

run client to reqeust server

```
$ ./build/tls_cnt
cnt start connect: 127.0.0.1:8080
cnt connected with chann 127.0.0.1:8080
cnt send request to serevr with ret: 57, wanted: 57
---
GET / HTTP/1.1
User-Agent: MNet/OpenSSL
Accept: */*


---
cnt recv response: 88
---
HTTP/1.1 200 OK
Content-Type: text/plain
Content-Length: 18

Hello MNet/OpenSSL


---
```

# LuaJIT wrapper

## Server

After `make openssl`, export `MNET_OPENSSL_DIR` and `LD_LIBRARY_PATH`, you need export `LUA_PATH` and `LUA_CPATH` before run luajit examples

```sh
$ export LUA_PATH=$PWD/extension/luajit/?.lua
$ export LUA_CPATH=$PWD/build/?.so
$ luajit examples/openssl/tls_web_svr.lua
```

then you can visit `https://127.0.0.1:8080` with browser, or

```sh
$ curl -k https://127.0.0.1:8080
hello, world !
```

## Client

try connect https://www.baidu.com as https client

```sh
$ luajit examples/openssl/tls_web_cnt.lua https://www.baidu.com
### using jit	Lua 5.1
### try connect https://www.baidu.com as 14.215.177.38:443
### config TLS context
### connected server
### send request length: 	114
### recv data:
----------
HTTP/1.1 200 OK
...
</html>
----------
### close socket
### bye
```


# Test

## reconnect

test connecting/connected/disconnecting/disconnected state changing.

run server as

```sh
$ ./build/tls_test_reconnect -s
```

run client as

```sh
$ ./build/tls_test_reconnect -c
```

## rwdata

test read/write a lot of specific sequence data.

run server as

```sh
$ ./build/tls_test_rwdata -s
```

run client as

```sh
$ ./build/tls_test_rwdata -c
```

# Usage

You need CA, server cert and private keys, the example code using self assign CA in example/openssl/.

Create your own SSL_CTX, and set config with `mnet_tls_config(SSL_CTX *ctx)`, then create chann with CHANN_TYPE_TLS.

That's all.

- examples/openssl/tls_svr.c using pull style API
- examples/openssl/tls_cnt.c using callback style API