TEST_MODE = DB_TEST
HOST_SYSTEM = $(shell uname | cut -f 1 -d_)
SYSTEM ?= $(HOST_SYSTEM)

BRPC_INCLUDE = ../../third_party/brpc/output/include
BRPC_LIB = ../../third_party/brpc/output/lib

CXX = g++
CPPFLAGS += `pkg-config --cflags protobuf` -g
CXXFLAGS += -std=c++11 -I.. -I${BRPC_INCLUDE} -D${TEST_MODE} -O2 

ifeq ($(SYSTEM),Darwin)
LDFLAGS += -L/usr/local/lib `pkg-config --libs protobuf`\
           -ldl
else
LDFLAGS += -L/usr/local/lib `pkg-config --libs protobuf`\
           -Wl,--no-as-needed -Wl,--as-needed\
           -ldl
endif

LDFLAGS += ../common/libcommon.a -L${BRPC_LIB}
LIBS =  -lpthread -lm -lbrpc

vpath %.proto $(PROTOS_PATH)

all: TpccClient

objects = tpcc_client.o \
					tpcc_proc.o \
					tpcc_workload.o  \
					tpcc_runner.o \
					main.o 

TpccClient: ${objects} 
	$(CXX) $^ $(LDFLAGS) $(LIBS) -o $@


clean:
	rm -f *.o *.pb.cc *.pb.h TpccClient
