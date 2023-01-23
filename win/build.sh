#
# build script under MSYS2
#

CC=gcc
CPP=g++

CFLAGS=' -Wall -std=c99 -Wdeprecated-declarations -D_POSIX_C_SOURCE=200112L -DWEPOLL_EXPRIMENT'
CPPFLAGS=' -Wall -Wdeprecated-declarations -Wno-deprecated'

OPENSSL_DIR='include'
MNET_LIBNAME='mnet.dll'

DEBUG=' -g'
RELEASE=' -O2'

LIB_SRCS=" ../src/*.c ../extension/mdns_utils/mdns_utils.c"
E_SRCS=" ../examples/*.c"
T_SRCS=" ../test/*.c"

OE_SRCS="../examples/openssl/*.c"
OL_SRCS="../extension/openssl/*.c"
OT_SRCS="../test/openssl/*.c"

CPP_SRCS="../test/*.cpp ../examples/*.cpp"

DIRS="../src/ ../extension/openssl/"
INCS="-I../src/ -I../extension/openssl/"
LIBS="-Lout -lmnet"

O_INCS="-Iopenssl/include"
O_DIRS="-Lopenssl/lib -Lout"
O_LIBS=" -lssl -lcrypto -lmnet"

OUT="out"

# build
#
sh clean.sh

# lib
#echo $CC $RELEASE $CFLAGS $INCS -o $OUT/$MNET_LIBNAME $LIB_SRCS -lWs2_32 -shared -fPIC
#$CC $RELEASE $CFLAGS $INCS -o $OUT/$MNET_LIBNAME $LIB_SRCS -lWs2_32 -shared -fPIC

# openssl lib
echo $CC $RELEASE $CFLAGS $INCS $O_INCS $O_DIRS -o $OUT/$MNET_LIBNAME $LIB_SRCS $OL_SRCS -lWs2_32 -shared -fPIC $O_LIBS
$CC $RELEASE $CFLAGS $INCS $O_INCS $O_DIRS -o $OUT/$MNET_LIBNAME $LIB_SRCS $OL_SRCS -lWs2_32 -shared -fPIC $O_LIBS

# openssl test
echo $CC $DEBUG $CFLAGS $INCS $O_INCS $O_DIRS -o $OUT/tls_test_reconnect $OT_SRCS $O_LIBS -DMNET_TLS_TEST_RECONNECT_C
$CC $DEBUG $CFLAGS $INCS $O_INCS $O_DIRS -o $OUT/tls_test_reconnect $OT_SRCS $O_LIBS -DMNET_TLS_TEST_RECONNECT_C

echo $CC $DEBUG $CFLAGS $INCS $O_INCS $O_DIRS -o $OUT/tls_test_rwdata $OT_SRCS $O_LIBS -DMNET_TLS_TEST_RWDATA_C
$CC $DEBUG $CFLAGS $INCS $O_INCS $O_DIRS -o $OUT/tls_test_rwdata $OT_SRCS $O_LIBS -DMNET_TLS_TEST_RWDATA_C

# test
echo $CC $DEBUG $CFLAGS $INCS -o $OUT/test_timer_c.out $T_SRCS $LIBS -DTEST_TIMER_C
$CC $DEBUG $CFLAGS $INCS -o $OUT/test_timer_c.out $T_SRCS $LIBS -DTEST_TIMER_C

# example_c
echo $CC $DEBUG $CFLAGS $INCS -o $OUT/ntp.out $E_SRCS $LIBS -DEXAMPLE_NTP
$CC $DEBUG $CFLAGS $INCS -o $OUT/ntp.out $E_SRCS $LIBS -DEXAMPLE_NTP

echo $CC $DEBUG $CFLAGS $INCS -o $OUT/echo_cnt.out $E_SRCS $LIBS -DEXAMPLE_ECHO_CNT_C
$CC $DEBUG $CFLAGS $INCS -o $OUT/echo_cnt.out $E_SRCS $LIBS -DEXAMPLE_ECHO_CNT_C

# example_cpp
echo $CPP $DEBUG $CPPFLAGS $INCS --std=c++0x -o $OUT/echo_svr.out $CPP_SRCS $LIBS -DEXAMPLE_ECHO_SVR_CPP
$CPP $DEBUG $CPPFLAGS $INCS --std=c++0x -o $OUT/echo_svr.out $CPP_SRCS $LIBS -DEXAMPLE_ECHO_SVR_CPP
