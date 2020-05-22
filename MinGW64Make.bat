mkdir build
gcc -O2 -Isrc src\mnet_core.c src\wepoll.c -lws2_32 -shared -fPIC -o build\mnet.dll -DWEPOLL_EXPRIMENT

gcc -O2 -Isrc -Lbuild examples\ntp.c -lws2_32 -lmnet -DEXAMPLE_NTP -o build\ntp.exe
g++ -O2 -Isrc -Lbuild examples\echo_cnt.cpp -lws2_32 -lmnet -DEXAMPLE_ECHO_CNT -o build\echo_cnt.exe
g++ -O2 -Isrc -Lbuild examples\echo_svr.cpp -lws2_32 -lmnet -DEXAMPLE_ECHO_SVR -o build\echo_svr.exe

g++ -O2 -Isrc -Lbuild test\test_reconnect.cpp -lws2_32 -lmnet -DTEST_RECONNECT -o build\test_reconnect.exe
gcc -O2 -Isrc -Lbuild test\test_reconnect_pull_style.c -lws2_32 -lmnet -DTEST_RECONNECT_PULL_STYLE -o build\test_reconnect_pull_style.exe

g++ -O2 -Isrc -Lbuild test\test_rwdata.cpp -lws2_32 -lmnet -DTEST_RWDATA -o build\test_rwdata.exe
gcc -O2 -Isrc -Lbuild test\test_rwdata_pull_style.c -lws2_32 -lmnet -DTEST_RWDATA_PULL_STYLE -o build\test_rwdata_pull_style.exe

g++ -O2 -Isrc -Lbuild test\test_timer.cpp -lws2_32 -lmnet -DTEST_TIMER -o build\test_timer.exe
gcc -O2 -Isrc -Lbuild test\test_timer_pull_style.c -lws2_32 -lmnet -DTEST_TIMER_PULL_STYLE -o build\test_timer_pull_style.exe
