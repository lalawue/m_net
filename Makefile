
CC=gcc
CFLAGS= -g -Wall -std=c99 -Wdeprecated-declarations
LIBS= -lc

SRCS := $(shell find src -name "*.c")
SRCS += $(shell find example -name "*.c")

DIRS := $(shell find src -type d)

INCS := $(foreach n, $(DIRS), -I$(n))

all: echo.out

echo.out: $(SRCS)
	$(CC) $(CFLAGS) $(INCS) -o example/$@ $^ $(LIBS) -DEXAMPLE_ECHO

clean:
	rm -rf example/*.out test/*.out example/*.dSYM test/*.dSYM
