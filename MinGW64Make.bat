mkdir build
gcc -O2 -Isrc src\mnet_core.c -lws2_32 -shared -fPIC -o build\libmnet.dll
