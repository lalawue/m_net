
CC=gcc
CFLAGS= -g -Wall -std=c99 -Wdeprecated-declarations

CPP=g++
CPPFLAGS= -g -Wall -Wdeprecated-declarations

LIBS= -lc

C_SRCS := $(shell find src -name "*.c")
C_SRCS += $(shell find examples -name "*.c")

CPP_SRCS := $(shell find src -name "*.c")
CPP_SRCS += $(shell find test -name "*.cpp")

DIRS := $(shell find src -type d)

INCS := $(foreach n, $(DIRS), -I$(n))

all: echo.out ntp.out multichann.out

echo.out: $(C_SRCS)
	$(CC) $(CFLAGS) $(INCS) -o examples/$@ $^ $(LIBS) -DEXAMPLE_ECHO

ntp.out: $(C_SRCS)
	$(CC) $(CFLAGS) $(INCS) -o examples/$@ $^ $(LIBS) -DEXAMPLE_NTP

multichann.out: $(CPP_SRCS)
	$(CPP) $(CPPFLAGS) $(INCS) -o test/$@ $^ $(LIBS) -DTEST_MULTICHANNS

clean:
	rm -rf examples/*.out test/*.out examples/*.dSYM test/*.dSYM
