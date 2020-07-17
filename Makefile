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
else
	MNET_LIBNAME=libmnet.so
endif

CFLAGS= -Wall -std=c99 -Wdeprecated-declarations -D_POSIX_C_SOURCE=200112L
CPPFLAGS= -Wall -Wdeprecated-declarations -Wno-deprecated

DEBUG= -g
RELEASE= -O2

LIBS= -lc -lmnet -Lbuild

LIB_SRCS := $(shell find src -name "*.c")
E_SRCS := $(shell find examples -name "*.c")
T_SRCS := $(shell find test -name "*.c")
L_SRCS := $(shell find extension/lua -name "*.c")

CPP_SRCS := $(shell find test -name "*.cpp")
CPP_SRCS += $(shell find examples -name "*.cpp")

DIRS := $(shell find src -type d)

INCS := $(foreach n, $(DIRS), -I$(n))

.PHONY : all
.PHONY : dir
.PHONY : debug_c
.PHONY : debug_cpp
.PHONY : lib
.PHONY : clean

all: _dir lib _debug_c _debug_cpp

_dir:
	mkdir -p build

lib: $(LIB_SRCS)
	mkdir -p build
	$(CC) $(RELEASE) $(CFLAGS) $(INCS) -o build/$(MNET_LIBNAME) $^ -lc -shared -fPIC

_debug_c: $(E_SRCS) $(T_SRCS)
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/ntp.out $^ $(LIBS) -DEXAMPLE_NTP
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/echo_svr_pull_style.out $^ $(LIBS) -DEXAMPLE_ECHO_SVR_PULL_STYLE
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/test_reconnect_pull_style.out $^ $(LIBS) -DTEST_RECONNECT_PULL_STYLE
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/test_rwdata_pull_style.out $^ $(LIBS) -DTEST_RWDATA_PULL_STYLE
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/test_timer_pull_style.out $^ $(LIBS) -DTEST_TIMER_PULL_STYLE

_debug_cpp: $(CPP_SRCS)
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) --std=c++0x -o build/echo_svr.out $^ $(LIBS) -DEXAMPLE_ECHO_SVR
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) -o build/echo_cnt.out $^ $(LIBS) -DEXAMPLE_ECHO_CNT
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) --std=c++0x -o build/test_reconnect.out $^ $(LIBS) -DTEST_RECONNECT
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) --std=c++0x -o build/test_rwdata.out $^ $(LIBS) -DTEST_RWDATA
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) --std=c++0x -o build/test_timer.out $^ $(LIBS) -DTEST_TIMER

luajit: lib

lua: lib
	$(CC) $(RELEASE) $(CFLAGS) $(INCS) $(LUA_INCS) $(LUA_LIBS) -o build/mnet.so $(L_SRCS) -shared -fPIC -llua -lmnet -Lbuild -I/usr/local/include/lua5.3

clean:
	rm -rf build
