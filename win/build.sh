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
LIBS=' -lWs2_32 -lmnet -Lbuild'

LIB_SRCS=" ../src/*.c ../extension/mdns_utils/mdns_utils.c"
E_SRCS=" ../examples/*.c"
T_SRCS=" ../test/*.c"

OE_SRCS="../examples/openssl/*.c"
OL_SRCS="../extension/openssl/*.c"
OT_SRCS="../test/openssl/*.c"

CPP_SRCS="../test/*.cpp ../examples/*.cpp"

DIRS="../src/ ../extension/openssl/"
INCS="-I../src/ -I../extension/openssl/"

O_INCS="-Iopenssl/include"
O_DIRS="-Lopenssl/lib -Lout"
O_LIBS=" -lssl -lcrypto"

OUT="out"

# build
#
sh clean.sh

# lib
#echo $CC $RELEASE $CFLAGS $INCS -o $OUT/$MNET_LIBNAME $LIB_SRCS -lWs2_32 -shared -fPIC
#$CC $RELEASE $CFLAGS $INCS -o $OUT/$MNET_LIBNAME $LIB_SRCS -lWs2_32 -shared -fPIC

# pull
#echo $CC $DEBUG $CFLAGS $INCS -o $OUT/ntp.out $E_SRCS $LIBS -DEXAMPLE_NTP
#$CC $DEBUG $CFLAGS $INCS -o $OUT/ntp.out $E_SRCS $LIBS -DEXAMPLE_NTP

#echo $CC $DEBUG $CFLAGS $INCS -o $OUT/echo_svr_pull_style.out $E_SRCS $LIBS -DEXAMPLE_ECHO_SVR_PULL_STYLE
#$CC $DEBUG $CFLAGS $INCS -o $OUT/echo_svr_pull_style.out $E_SRCS $LIBS -DEXAMPLE_ECHO_SVR_PULL_STYLE

# callback
#echo $CPP $DEBUG $CPPFLAGS $INCS --std=c++0x -o $OUT/echo_cnt.out $CPP_SRCS $LIBS -DEXAMPLE_ECHO_CNT
#$CPP $DEBUG $CPPFLAGS $INCS --std=c++0x -o $OUT/echo_cnt.out $CPP_SRCS $LIBS -DEXAMPLE_ECHO_CNT

# openssl
echo $CC $RELEASE $CFLAGS $INCS $O_INCS $O_DIRS -o $OUT/$MNET_LIBNAME $LIB_SRCS $OL_SRCS -lWs2_32 -shared -fPIC $O_LIBS
$CC $RELEASE $CFLAGS $INCS $O_INCS $O_DIRS -o $OUT/$MNET_LIBNAME $LIB_SRCS $OL_SRCS -lWs2_32 -shared -fPIC $O_LIBS

echo $CC $DEBUG $CFLAGS $INCS $O_INCS $O_DIRS -o $OUT/tls_test_reconnect $OT_SRCS $O_LIBS -lmnet -DMNET_TLS_TEST_RECONNECT_PULL_STYLE
$CC $DEBUG $CFLAGS $INCS $O_INCS $O_DIRS -o $OUT/tls_test_reconnect $OT_SRCS $O_LIBS -lmnet -DMNET_TLS_TEST_RECONNECT_PULL_STYLE

echo $CC $DEBUG $CFLAGS $INCS $O_INCS $O_DIRS -o $OUT/tls_test_rwdata $OT_SRCS $O_LIBS -lmnet -DMNET_TLS_TEST_RWDATA_PULL_STYLE
$CC $DEBUG $CFLAGS $INCS $O_INCS $O_DIRS -o $OUT/tls_test_rwdata $OT_SRCS $O_LIBS -lmnet -DMNET_TLS_TEST_RWDATA_PULL_STYLE
