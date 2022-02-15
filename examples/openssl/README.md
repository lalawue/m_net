
this dir contains codes for example about how to use mnet_openssl extension.

## Build

first export MNET_OPENSSL_DIR to openssl library dir, below is example for I `brew install openssl` in MacOS.

```
$ export MNET_OPENSSL_DIR=/usr/local/Cellar/openssl@1.1/1.1.1k/
```

## Run

first export library path

```
$ exoprt LD_LIBARY_PATH=/usr/local/Cellar/openssl@1.1/1.1.1k/
$ export DY_LD_LIBARY_PATH=/usr/local/Cellar/openssl@1.1/1.1.1k/
```

run server like

```
$ ./build/openssl_svr 127.0.0.1:8080
config openssl ctx.
svr start listen: 127.0.0.1:8080
svr accept cnt with chann 127.0.0.1:50285
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

run client like
```
$ ./build/openssl_cnt
config openssl ctx.
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

## Usage

You need CA, server cert and private keys, or google how to get it. The example code using self assign CA in example/openssl/.

And use `mnet_openssl_chann_` instead of `mnet_chann_`, except for `mnet_chann_socket_` and tools API.

- examples/openssl/openssl_svr.c using pull style API
- examples/openssl/openssl_cnt.c using callback style API