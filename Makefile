
CC=gcc
CFLAGS= -Wall -std=c99 -Wdeprecated-declarations

CPP=g++
CPPFLAGS= -Wall -Wdeprecated-declarations

DEBUG= -g
RELEASE= -O2

LIBS= -lc

LIB_SRCS := $(shell find src -name "*.c")
C_SRCS := $(shell find examples -name "*.c")
CPP_SRCS := $(shell find test -name "*.cpp")

DIRS := $(shell find src -type d)

INCS := $(foreach n, $(DIRS), -I$(n))

all: dir debug_c debug_cpp lib

dir:
	mkdir -p build

debug_c: $(LIB_SRCS) $(C_SRCS)
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/echo.out $^ $(LIBS) -DEXAMPLE_ECHO
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/ntp.out $^ $(LIBS) -DEXAMPLE_NTP

debug_cpp: $(LIB_SRCS) $(CPP_SRCS)
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) -o build/multichann.out $^ $(LIBS) -DTEST_MULTICHANNS
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) -o build/reconnect.out $^ $(LIBS) -DTEST_RECONNECT

lib: $(LIB_SRCS)
	$(CC) $(RELEASE) $(CFLAGS) $(INCS) -o build/libmnet.dylib $^ $(LIBS) -shared

clean:
	rm -rf build
