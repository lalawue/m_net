
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

all: debug_c debug_cpp

debug_c: $(LIB_SRCS) $(C_SRCS)
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o examples/echo.out $^ $(LIBS) -DEXAMPLE_ECHO
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o examples/ntp.out $^ $(LIBS) -DEXAMPLE_NTP

debug_cpp: $(LIB_SRCS) $(CPP_SRCS)
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) -o test/multichann.out $^ $(LIBS) -DTEST_MULTICHANNS
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) -o test/reconnect.out $^ $(LIBS) -DTEST_RECONNECT

lib: $(LIB_SRCS)
	$(CC) $(RELEASE) $(CFLAGS) $(INCS) -o libmnet.dylib $^ $(LIBS) -shared

clean:
	rm -rf examples/*.out test/*.out examples/*.dSYM test/*.dSYM *.dylib
