
CC=gcc
CFLAGS= -Wall -std=c99 -Wdeprecated-declarations

CPP=g++
CPPFLAGS= -Wall -Wdeprecated-declarations -Wno-deprecated

DEBUG= -g
RELEASE= -O2

LIBS= -lc

LIB_SRCS := $(shell find src -name "*.c")
C_SRCS := $(shell find examples -name "*.cpp")
CPP_SRCS := $(shell find test -name "*.cpp")

DIRS := $(shell find src -type d)

INCS := $(foreach n, $(DIRS), -I$(n))

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
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) --std=c++0x -o build/echo_svr.out $^ $(LIBS) -DEXAMPLE_ECHO_SVR
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) -o build/echo_cnt.out $^ $(LIBS) -DEXAMPLE_ECHO_CNT
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) -o build/ntp.out $^ $(LIBS) -DEXAMPLE_NTP

_debug_cpp: $(LIB_SRCS) $(CPP_SRCS)
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) --std=c++0x -o build/multichann.out $^ $(LIBS) -DTEST_MULTICHANNS
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) -o build/reconnect.out $^ $(LIBS) -DTEST_RECONNECT

_lib: $(LIB_SRCS)
	$(CC) $(RELEASE) $(CFLAGS) $(INCS) -o build/libmnet.dylib $^ $(LIBS) -shared -fPIC

clean:
	rm -rf build
