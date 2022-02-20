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
OPENSSL_DIR=$MNET_OPENSSL_DIR

DEBUG= -g
RELEASE= -O2

LIBS= -lc -lmnet -Lbuild

LIB_SRCS := $(shell find src -name "*.c")
LIB_SRCS += $(shell find extension/mdns_utils -name "*.c")
E_SRCS := $(shell find examples -maxdepth 1 -name "*.c")
T_SRCS := $(shell find test -name "*.c")
OE_SRCS := $(shell find examples/openssl -name "*.c")
OL_SRCS := $(shell find extension/openssl -name "*.c")

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
.PHONY : pull
.PHONY : callback
.PHONY : openssl
.PHONY : clean

all:
	@echo "try make with these"
	@echo "$$ make clean"
	@echo "$$ make lib		# only make library"
	@echo "$$ make pull		# make pull style example"
	@echo "$$ make callback		# make callback style example"
	@echo "$$ make openssl		# make openssl example"

lib: $(LIB_SRCS)
	@mkdir -p build
	$(CC) $(RELEASE) $(CFLAGS) $(INCS) -o build/$(MNET_LIBNAME) $^ -lc -shared -fPIC

pull: $(E_SRCS) $(T_SRCS)
	@mkdir -p build
	$(CC) $(RELEASE) $(CFLAGS) $(INCS) -o build/$(MNET_LIBNAME) $(LIB_SRCS) -lc -shared -fPIC
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/ntp.out $^ $(LIBS) -DEXAMPLE_NTP
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/echo_svr_pull_style.out $^ $(LIBS) -DEXAMPLE_ECHO_SVR_PULL_STYLE
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/test_reconnect_pull_style.out $^ $(LIBS) -DTEST_RECONNECT_PULL_STYLE
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/test_rwdata_pull_style.out $^ $(LIBS) -DTEST_RWDATA_PULL_STYLE
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) -o build/test_timer_pull_style.out $^ $(LIBS) -DTEST_TIMER_PULL_STYLE

callback: $(CPP_SRCS)
	@mkdir -p build
	$(CC) $(RELEASE) $(CFLAGS) $(INCS) -o build/$(MNET_LIBNAME) $(LIB_SRCS) -lc -shared -fPIC
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) --std=c++0x -o build/echo_svr.out $^ $(LIBS) -DEXAMPLE_ECHO_SVR
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) -o build/echo_cnt.out $^ $(LIBS) -DEXAMPLE_ECHO_CNT
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) --std=c++0x -o build/test_reconnect.out $^ $(LIBS) -DTEST_RECONNECT
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) --std=c++0x -o build/test_rwdata.out $^ $(LIBS) -DTEST_RWDATA
	$(CPP) $(DEBUG) $(CPPFLAGS) $(INCS) --std=c++0x -o build/test_timer.out $^ $(LIBS) -DTEST_TIMER

openssl: $(OE_SRCS) $(OL_SRCS)
	@mkdir -p build
	@echo "export MNET_OPENSSL_DIR=$(MNET_OPENSSL_DIR)"
	$(CC) $(RELEASE) $(CFLAGS) $(INCS) $(O_INCS) $(O_DIRS) -o build/$(MNET_LIBNAME) $^ $(LIB_SRCS) -lc -shared -fPIC $(O_LIBS)
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) $(O_INCS) $(O_DIRS) -o build/tls_svr $^ $(O_LIBS) -lmnet -DMNET_OPENSSL_SVR
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) $(O_INCS) $(O_DIRS) -o build/tls_cnt $^ $(O_LIBS) -lmnet -DMNET_OPENSSL_CNT
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) $(O_INCS) $(O_DIRS) -o build/tls_reconnect $^ $(O_LIBS) -lmnet -DMNET_OPENSSL_TEST_RECONNECT_PULL_STYLE
	$(CC) $(DEBUG) $(CFLAGS) $(INCS) $(O_INCS) $(O_DIRS) -o build/tls_rwdata $^ $(O_LIBS) -lmnet -DMNET_OPENSSL_TEST_RWDATA_PULL_STYLE

clean:
	rm -rf build
	find . -name "*.so" -exec rm {} \;
	find . -name "*.o" -exec rm {} \;