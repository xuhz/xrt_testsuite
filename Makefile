CC     = g++
XILINX_XRT = /opt/xilinx/xrt
CFLAGS = -g -std=c++14 -Wall -I ${XILINX_XRT}/include 
LFLAGS = -lstdc++ -lxrt_coreutil -lxrt_core -lrt -luuid -pthread -L ${XILINX_XRT}/lib

OBJ = hello.o

TGT+=host.exe

%.o: %.cpp
	$(CC) -c -o $@ $< $(CFLAGS)

all: $(TGT)

$(TGT): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LFLAGS)

clean:
	rm -f core *.o $(TGT)

