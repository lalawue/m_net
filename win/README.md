
# About

this is windows building dir under MSYS2.

you can edit `build.sh` for further configuration.


## Building

under MSYS2 bash shell, just 

```sh
$ sh build.sh
```

if you need OpenSSL, read OpenSSL section to copy required resoureces.


## Runing

Win+R open cmd.exe, run server

```sh
> run.bat out\tls_test_reconnect.exe -s
```

Win+R open another cmd.exe, run client side

```sh
> run.bat out\tls_test_reconnect.exe -c
```

and you can test `out\tls_test_rwdata.exe`.


## OpenSSL

the project contains an pre-compiled OpenSSL bundle under Win10, you can get it from release page.

or you can install lastest OpenSSL from [https://slproweb.com/products/Win32OpenSSL.html](https://slproweb.com/products/Win32OpenSSL.html), 

after install to `C:\Program Files\OpenSSL-Win64`, copy lib and headers to `openssl/` dir

- copy `libcrypto` and `libssl` into `openssl/lib/` dir, link to short name.
- copy `include/` header dir into `openssl/` dir.