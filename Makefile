
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S), FreeBSD)
CC=cc
CPP=c++
else
CC=gcc
CPP=g++
endif

CFLAGS= -Wall -std=gnu99 -Wdeprecated-declarations
CPPFLAGS= -Wall -Wdeprecated-declarations -Wno-deprecated

DEBUG= -g
RELEASE= -O2

LIBS= -lc

LIB_SRCS := $(shell find src -name "*.c")
C_SRCS := $(shell find examples -name "*.c")
CPP_SRCS := $(shell find test -name "*.cpp")
CPP_SRCS += $(shell find examples -name "*.cpp")



DIRS := $(shell find src -type d)

INCS := $(foreach n, $(DIRS), -I$(n))

LUA_SRCS := $(shell find extension/lua -name "*.c")
LUA_INCS := -I/usr/local/include
LUA_LIBS := -L/usr/local/lib -llua

.PHONY : all
.PHONY : dir
.PHONY : debug_c
.PHONY : debug_cpp
.PHONY : lib
.PHONY : clean

all: _dir _debug_c _debug_cpp _lib

_dir:
	mkdir -p build

_debug_c: $(LIB_SRCS) $(C_SRCS)
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/ntp.out $^ $(LIBS) -DEXAMPLE_NTP

_debug_cpp: $(LIB_SRCS) $(CPP_SRCS)
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) --std=c++0x -o build/echo_svr.out $^ $(LIBS) -DEXAMPLE_ECHO_SVR
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) -o build/echo_cnt.out $^ $(LIBS) -DEXAMPLE_ECHO_CNT
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) --std=c++0x -o build/multichann.out $^ $(LIBS) -DTEST_MULTICHANNS
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) --std=c++0x -o build/reconnect.out $^ $(LIBS) -DTEST_RECONNECT
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) --std=c++0x -o build/rwdata.out $^ $(LIBS) -DTEST_RWDATA

_lib: $(LIB_SRCS)
	$(CC) $(RELEASE) $(CFLAGS) $(INCS) -o build/libmnet.dylib $^ $(LIBS) -shared -fPIC

lua: $(LUA_SRCS) $(LIB_SRCS)
	mkdir -p build
	$(CC) $(RELEASE) $(CFLAGS) $(INCS) $(LUA_INCS) $(LUA_LIBS) -o build/mnet.so $^ -shared -fPIC

clean:
	rm -rf build
