CC     = g++
XILINX_XRT = /opt/xilinx/xrt
CFLAGS = -g -std=c++14 -Wall -I ${XILINX_XRT}/include 
LFLAGS = -lstdc++ -lxrt_coreutil -lxrt_core -lrt -luuid -lboost_system -lboost_filesystem -pthread -L ${XILINX_XRT}/lib

OBJ = null_kernel.o

TGT+=null_kernel.exe

%.o: %.cpp
	$(CC) -c -o $@ $< $(CFLAGS)

all: $(TGT)

$(TGT): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LFLAGS)

clean:
	rm -f core *.o $(TGT)

