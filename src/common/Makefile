MODE = DB_TEST
HOST_SYSTEM = $(shell uname | cut -f 1 -d_)
SYSTEM ?= $(HOST_SYSTEM)
CXX = g++
CPPFLAGS += `pkg-config --cflags protobuf` -Wno-deprecated-register -Wno-format -Wno-unused-variable
CXXFLAGS += -std=c++11 -Wall -I../../third_party/mintomic/include -D${MODE} -g -O2

PROTOC = protoc

PROTOS_PATH = .

vpath %.proto $(PROTOS_PATH)

all: libcommon.a

test: libcommon.a test.cpp
	${CXX} -g libcommon.a test.cpp ${CXXFLAGS} ${LDFLAGS} -o  test

objects = db.pb.o \
			murmur_hash.o \
			hashtable1.o \
			worker.o \
			global.o \
			debuglog.o \
			pthread_barrier_t.o \
			config.o

libcommon.a: ${objects} 
	ar -cvq libcommon.a ${objects}

%.pb.cc %.pb.h:%.proto
	@echo "Generating $@"
	@$(PROTOC) --cpp_out=. --proto_path=. $(PROTOC_EXTRA_ARGS) $<

clean:
	rm -f *.o *.pb.cc *.pb.h  *.a

PROTOC_CMD = which $(PROTOC)
PROTOC_CHECK_CMD = $(PROTOC) --version | grep -q libprotoc.2
