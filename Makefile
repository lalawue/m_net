#
# use gmake in FreeBSD

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S), FreeBSD)
	CC=cc
	CPP=c++
else
	CC=gcc
	CPP=g++
endif

ifeq ($(UNAME_S), Darwin)
	MNET_LIBNAME=libmnet.dylib
    LUA_LIBNAME=mnet.so
	EXTRA_LIBS=
else
	MNET_LIBNAME=libmnet.so
    LUA_LIBNAME=mnet.so
	EXTRA_LIBS=-lpthread
endif

CFLAGS= -Wall -std=c99 -Wdeprecated-declarations -D_POSIX_C_SOURCE=200112L
CPPFLAGS= -Wall -Wdeprecated-declarations -Wno-deprecated
OPENSSL_DIR=$MNET_OPENSSL_DIR

DEBUG= -g
RELEASE= -O2

LIBS= -lc -lmnet -Lbuild ${EXTRA_LIBS}

LIB_SRCS := $(shell find src -name "*.c")
LIB_SRCS += $(shell find extension/mdns -name "*.c")

E_SRCS := $(shell find examples -maxdepth 1 -name "*.c")
E_SRCS += $(shell find examples/process -maxdepth 1 -name "*.c")
T_SRCS := $(shell find test -maxdepth 1 -name "*.c")

OE_SRCS := $(shell find examples/openssl -name "*.c")
OL_SRCS := $(shell find extension/openssl -name "*.c")
OT_SRCS := $(shell find test/openssl -name "*.c")

CPP_SRCS := $(shell find test -name "*.cpp")
CPP_SRCS += $(shell find examples -name "*.cpp")

DIRS := $(shell find src -type d)
DIRS += $(shell find extension/openssl -type d)

INCS := $(foreach n, $(DIRS), -I$(n))

O_INCS := -I$(MNET_OPENSSL_DIR)/include
O_DIRS := -L$(MNET_OPENSSL_DIR)/lib -Lbuild
O_LIBS := -lssl -lcrypto

.PHONY : all
.PHONY : lib
.PHONY : example_c
.PHONY : example_cpp
.PHONY : openssl
.PHONY : clean

all:
	@echo "try make with these"
	@echo "$$ make clean"
	@echo "$$ make lib		# only make library"
	@echo "$$ make example_c	# make C example"
	@echo "$$ make example_cpp	# make CPP example"
	@echo "$$ make openssl		# make openssl example"

lib: $(LIB_SRCS)
	@mkdir -p build
	$(CC) $(RELEASE) $(CFLAGS) $(INCS) -o build/$(MNET_LIBNAME) $^ -lc -shared -fPIC
	cd build && ln -sf $(MNET_LIBNAME) $(LUA_LIBNAME)

example_c: $(E_SRCS) $(T_SRCS)
	@mkdir -p build
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/$(MNET_LIBNAME) $(LIB_SRCS) -lc -shared -fPIC
	cd build && ln -sf $(MNET_LIBNAME) $(LUA_LIBNAME)
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/ntp.out $^ $(LIBS) -DEXAMPLE_NTP
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/echo_svr_c.out $^ $(LIBS) -DEXAMPLE_ECHO_SVR_C
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/echo_cnt_c.out $^ $(LIBS) -DEXAMPLE_ECHO_CNT_C
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/chann_web_c.out $^ $(LIBS) -DEXAMPLE_CHANN_WEB_C
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/echo_udp_svr_c.out $^ $(LIBS) -DEXAMPLE_ECHO_UDP_SVR_C
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/echo_udp_cnt_c.out $^ $(LIBS) -DEXAMPLE_ECHO_UDP_CNT_C
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/multi_process_svr_c.out $^ $(LIBS) -DEXAMPLE_MULTI_PROCESS_SVR_C
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/multi_process_cnt_c.out $^ $(LIBS) -DEXAMPLE_MULTI_PROCESS_CNT_C
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/test_reconnect_c.out $^ $(LIBS) -DTEST_RECONNECT_C
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/test_rwdata_c.out $^ $(LIBS) -DTEST_RWDATA_C
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/test_timer_c.out $^ $(LIBS) -DTEST_TIMER_C

example_cpp: $(CPP_SRCS)
	@mkdir -p build
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/$(MNET_LIBNAME) $(LIB_SRCS) -lc -shared -fPIC
	cd build && ln -sf $(MNET_LIBNAME) $(LUA_LIBNAME)
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) --std=c++0x -o build/echo_svr_cpp.out $^ $(LIBS) -DEXAMPLE_ECHO_SVR_CPP
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) --std=c++0x -o build/chann_web_cpp.out $^ $(LIBS) -DEXAMPLE_CHANN_WEB_CPP
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) --std=c++0x -o build/test_reconnect_cpp.out $^ $(LIBS) -DTEST_RECONNECT_CPP
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) --std=c++0x -o build/test_rwdata_cpp.out $^ $(LIBS) -DTEST_RWDATA_CPP
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) --std=c++0x -o build/test_timer_cpp.out $^ $(LIBS) -DTEST_TIMER_CPP

openssl: $(OE_SRCS) $(OL_SRCS) $(OT_SRCS)
	@mkdir -p build
	@echo "export MNET_OPENSSL_DIR=$(MNET_OPENSSL_DIR)"
	$(CC) $(RELEASE) $(CFLAGS) $(INCS) $(O_INCS) $(O_DIRS) -o build/$(MNET_LIBNAME) $^ $(LIB_SRCS) -lc -shared -fPIC $(O_LIBS)
	cd build && ln -sf $(MNET_LIBNAME) $(LUA_LIBNAME)
	$(CC) $(RELEASE) $(CFLAGS) $(INCS) $(O_INCS) $(O_DIRS) -o build/tls_svr $^ $(O_LIBS) -lmnet -DMNET_OPENSSL_SVR_C
	$(CC) $(RELEASE) $(CFLAGS) $(INCS) $(O_INCS) $(O_DIRS) -o build/tls_cnt $^ $(O_LIBS) -lmnet -DMNET_OPENSSL_CNT_C
	$(CC) $(RELEASE) $(CFLAGS) $(INCS) $(O_INCS) $(O_DIRS) -o build/tls_test_reconnect $^ $(O_LIBS) -lmnet -DMNET_TLS_TEST_RECONNECT_C
	$(CC) $(RELEASE) $(CFLAGS) $(INCS) $(O_INCS) $(O_DIRS) -o build/tls_test_rwdata $^ $(O_LIBS) -lmnet -DMNET_TLS_TEST_RWDATA_C

clean:
	rm -rf build
	find . -name "*.so" -exec rm {} \;
	find . -name "*.o" -exec rm {} \;
