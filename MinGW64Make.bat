mkdir build
gcc -O2 -Isrc src\mnet_core.c -lws2_32 -shared -fPIC -o build\mnet.dll
gcc -O2 -Isrc src\mnet_core.c examples\echo_cnt.cpp -lws2_32 -DEXAMPLE_ECHO_CNT -o build\echo_cnt.exe
g++ -O2 -Isrc src\mnet_core.c examples\echo_svr.cpp -lws2_32 -DEXAMPLE_ECHO_SVR -o build\echo_svr.exe
