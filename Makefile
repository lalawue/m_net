
CC=gcc
CFLAGS= -g -Wall -std=c99 -Wdeprecated-declarations
LIBS= -lc

SRCS := $(shell find src -name "*.c")
SRCS += $(shell find examples -name "*.c")

DIRS := $(shell find src -type d)

INCS := $(foreach n, $(DIRS), -I$(n))

all: echo.out ntp.out

echo.out: $(SRCS)
	$(CC) $(CFLAGS) $(INCS) -o examples/$@ $^ $(LIBS) -DEXAMPLE_ECHO

ntp.out: $(SRCS)
	$(CC) $(CFLAGS) $(INCS) -o examples/$@ $^ $(LIBS) -DEXAMPLE_NTP

clean:
	rm -rf examples/*.out test/*.out examples/*.dSYM test/*.dSYM
