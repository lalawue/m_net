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

CFLAGS= -Wall -std=gnu99 -Wdeprecated-declarations
CPPFLAGS= -Wall -Wdeprecated-declarations -Wno-deprecated

DEBUG= -g
RELEASE= -O2

LIBS= -lc

LIB_SRCS := $(shell find src -name "*.c")
C_SRCS := $(shell find examples -name "*.c")
C_SRCS += $(shell find test -name "*.c")

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

all: _dir _debug_c _debug_cpp lib

_dir:
	mkdir -p build

_debug_c: $(LIB_SRCS) $(C_SRCS)
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/ntp.out $^ $(LIBS) -DEXAMPLE_NTP
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/echo_svr_pull_style.out $^ $(LIBS) -DEXAMPLE_ECHO_SVR_PULL_STYLE
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/test_rwdata_pull_style.out $^ $(LIBS) -DTEST_RWDATA_PULL_STYLE

_debug_cpp: $(LIB_SRCS) $(CPP_SRCS)
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) --std=c++0x -o build/echo_svr.out $^ $(LIBS) -DEXAMPLE_ECHO_SVR
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) -o build/echo_cnt.out $^ $(LIBS) -DEXAMPLE_ECHO_CNT
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) --std=c++0x -o build/test_multichann.out $^ $(LIBS) -DTEST_MULTICHANNS
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) --std=c++0x -o build/test_reconnect.out $^ $(LIBS) -DTEST_RECONNECT
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) --std=c++0x -o build/test_rwdata.out $^ $(LIBS) -DTEST_RWDATA

lib: $(LIB_SRCS)
	mkdir -p build
	$(CC) $(RELEASE) $(CFLAGS) $(INCS) -o build/$(MNET_LIBNAME) $^ $(LIBS) -shared -fPIC

luajit: lib

clean:
	rm -rf build
