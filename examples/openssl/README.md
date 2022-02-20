

- [Build](#build)
- [Run](#run)
  - [Server](#server)
  - [Client](#client)
- [LuaJIT wrapper](#luajit-wrapper)
- [Test](#test)
  - [reconnect](#reconnect)
  - [rwdata](#rwdata)
- [Usage](#usage)


# Build

First export MNET_OPENSSL_DIR to openssl library dir, here is example for I `brew install openssl` under MacOS.

```
$ export MNET_OPENSSL_DIR=/usr/local/Cellar/openssl@1.1/1.1.1k/
$ make openssl
```

# Run

Config `LD_LIBRARY_PATH`

```
$ exoprt LD_LIBARY_PATH=/usr/local/Cellar/openssl@1.1/1.1.1k/
$ export DY_LD_LIBARY_PATH=/usr/local/Cellar/openssl@1.1/1.1.1k/
```

## Server

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

After `make openssl`, you need config `LUA_CPATH` and `LUA_PATH`:

```sh
$ export LUA_PATH=$PWD/extension/luajit/?.lua
$ export LUA_CPATH=$PWD/build/?.so
$ luajit examples/openss/tls_web.lua
```


# Test

## reconnect

run server as

```sh
$ ./build/tls_test_reconnect -s
```

run client as

```sh
$ ./build/tls_test_reconnect -c
```

## rwdata

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