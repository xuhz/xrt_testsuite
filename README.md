# xrt testsuite
## About
This test program can be used to run 

* Kernel execution throughput and latency test
* DMA bandwidth and latency test
* Multiple thead test
* Multiple process test 
* Single run with specified number of process, thread, executions or period of time, cmd queue
* length for kernel execution or bo size for DMA, throughput or latency 

The test is xrt native API based, having similar overhead to the opencl API based

## Known issues
* xrt native APIs not stable yet, so sometimes a xrt build change makes the compilation fail.
  eg. https://github.com/Xilinx/XRT/commit/5bb8fdf3ab97a168f5ca5b990f800ecceeda2f21

  The above PR makes the API change 
  from "wait(unsigned int timeout_ms) const"
  to "wait(const std::chrono::milliseconds& timeout_ms) const"

  If you see runtime error, like "./host.exe: undefined symbol: _ZNK3xrt3run4waitEj ",
  that probobally means the xrt version and the binary built don't match.
  
  This commit, '19afcc829f79efc1424b4a4f422160ed93a421f5' is the devide line. Before(inclusive) of
  which old API is used, after new API is used. 
   
 
## cmdline: 
```
$>./host.exe -h
usage:
  ./host.exe [options] -k <bitstream>

  options:
	-k <bitstream>, specifying path to xclbin file, mandatory 
	-b <bulk>, specifying cmd queue length per thread, optional,
	           default is minimum of 32 and number of executions (see -n)
	           cmd queue length number of cmds will be issued before polling cmd status,
	           this is the aka bulk submit, then afterwards, a new cmd will be issued only after one cmd is complete
	           when bulk size is 1, it is ping-pong test
	-d <index>, specifying index to FPGA device, optional, default is 0
	-n <count>, specifying number of kernel executions per thread, optional, defualt is 30000
	-s <bo size>, specifying size of BO, optional, default is 4k
	-t <threads>, specifying number of threads per process, optional, default is 1
	-p <processes>, specifying number of processes spawned, optional, default is 1
	-T <second>, specifying number of second the test will run, exclusive to -n, optional
       	-K <run type> optional, default is 2
	           1|dma: dma test
	           2|kernel: kernel execution test
	-D <dma dir> 0: to device, 1: from device. optional, default is bi-direction
	-m <mode>, optional, default is 4
	           1|tput: throughput test
	                   for kernel execution, run with different bulk size from 1 ,2, 4, all the way up to 256
	                   for dma test, run with bo size 16m, 64m, 256m
	                   only 1 process will be used in this case
	           2|mp:   multiple process test, run with different processes from 1 to the next of power of 2 of specified
	                     eg. -p 4, will run 1, 2, 4 processes
	                     eg. -p 9, will run 1, 2, 4, 8, 16 processes
	                   dma test doesn't support this mode
	           3|mt:   multiple thread test, run with different threads from 1 to the next of power of 2 of specified
	                     eg. -t 4, will run 1, 2, 4 threads
	                     eg. -t 9, will run 1, 2, 4, 8, 16 threads
	           4:      single run with specified -b, -n | -T, -t, -p, -L, -K
	-h, help

```
## Examples: 

### default run -- 1 process, 1 thread, cmd queue length 32, number of executions 30000
```
./host.exe -k /opt/xilinx/dsa/xilinx_u200_xdma_201830_2/test/verify.xclbin 
Test running...(pid: 11809)

kernel execution throughput:
	process(es): 1
	thread(s) per process: 1
	queue length: 32
	throughput: 95800.1 ops/s (30000 executions in 313.152 ms)
```
### kernel execution ping-pong test -- the cmd will not be issued before the previous cmd is complete
```
./host.exe -k /opt/xilinx/dsa/xilinx_u200_xdma_201830_2/test/verify.xclbin  -b 1
Test running...(pid: 11845)

kernel execution throughput:
	process(es): 1
	thread(s) per process: 1
	queue length: 1
	throughput: 16160.4 ops/s (30000 executions in 1856.39 ms)
```
### kernel execution throughput test -- throughput will be printed every second
```
./host.exe -k /opt/xilinx/dsa/xilinx_u200_xdma_201830_2/test/verify.xclbin -T 5 1
Test running...(pid: 46291, xclbin loaded in 36.2746 ms)
thread 0 running kernel name: hello:{hello_1}
	 ... 98270 ops/s
	 ... 98706 ops/s
	 ... 98645 ops/s
	 ... 98564 ops/s
	 ... 98811 ops/s

kernel execution throughput:
	process(es): 1
	thread(s) per process: 1
	queue length: 32
	throughput: 98594.5 ops/s (492996 executions in 5000.24 ms)
```
### run test on device 1, with 2 threads, for 5.1s
```
./host.exe -k /opt/xilinx/firmware/u25/gen3x8-xdma/base/test/verify.xclbin -d 1 -t 2 -T 5.1
Test running...(pid: 11897)

kernel execution throughput:
	process(es): 1
	thread(s) per process: 2
	queue length: 32
	throughput: 77144.4 ops/s (393501 executions in 5100.84 ms)

```
### kernel execution throughput test
```
./host.exe -k /opt/xilinx/dsa/xilinx_u200_xdma_201830_2/test/verify.xclbin -m tput

Throughput test...
Test running...(pid: 11925)

kernel execution throughput:
	process(es): 1
	thread(s) per process: 1
	queue length: 1
	throughput: 16699.6 ops/s (30000 executions in 1796.45 ms)
Test running...(pid: 11925)

kernel execution throughput:
	process(es): 1
	thread(s) per process: 1
	queue length: 2
	throughput: 33617.2 ops/s (30000 executions in 892.399 ms)
Test running...(pid: 11925)

kernel execution throughput:
	process(es): 1
	thread(s) per process: 1
	queue length: 4
	throughput: 59943.7 ops/s (30000 executions in 500.47 ms)
Test running...(pid: 11925)

kernel execution throughput:
	process(es): 1
	thread(s) per process: 1
	queue length: 8
	throughput: 83645.6 ops/s (30000 executions in 358.656 ms)
Test running...(pid: 11925)

kernel execution throughput:
	process(es): 1
	thread(s) per process: 1
	queue length: 16
	throughput: 91367.1 ops/s (30000 executions in 328.346 ms)
Test running...(pid: 11925)

kernel execution throughput:
	process(es): 1
	thread(s) per process: 1
	queue length: 32
	throughput: 96631.2 ops/s (30000 executions in 310.459 ms)
Test running...(pid: 11925)

kernel execution throughput:
	process(es): 1
	thread(s) per process: 1
	queue length: 64
	throughput: 96578.5 ops/s (30000 executions in 310.628 ms)
Test running...(pid: 11925)

kernel execution throughput:
	process(es): 1
	thread(s) per process: 1
	queue length: 128
	throughput: 97048.2 ops/s (30000 executions in 309.125 ms)
Test running...(pid: 11925)

kernel execution throughput:
	process(es): 1
	thread(s) per process: 1
	queue length: 256
	throughput: 95636.1 ops/s (30000 executions in 313.689 ms)

Max throughput: 97048.2 ops/s
@ processes: 1 / threads: 1 / cmd queue length: 128

``` 

### multiple process test
```
./host.exe -k /opt/xilinx/dsa/xilinx_u200_xdma_201830_2/test/verify.xclbin -m mp -p 4

Multiple process test...
Test running...(pid: 11957)

multiple process kernel execution throughput:
	processes: 1
	thread(s) per process: 1
	cmd queue length: 32
	throughput: 96208.9 ops/s (30000 executions in 311.821 ms)
Test running...(pid: 11959)
Test running...(pid: 11960)

multiple process kernel execution throughput:
	processes: 2
	thread(s) per process: 1
	cmd queue length: 32
	throughput: 94065.6 ops/s (60000 executions in 637.853 ms)
Test running...(pid: 11966)
Test running...(pid: 11964)
Test running...(pid: 11967)
Test running...(pid: 11965)

multiple process kernel execution throughput:
	processes: 4
	thread(s) per process: 1
	cmd queue length: 32
	throughput: 81129.5 ops/s (120000 executions in 1479.12 ms)

```

### multiple thread test
```
./host.exe -k /opt/xilinx/dsa/xilinx_u200_xdma_201830_2/test/verify.xclbin -m mt -t 5

Multiple thread test...
Roundup threads to 8(next power of 2)
Test running...(pid: 11992)

kernel execution throughput:
	process(es): 1
	thread(s) per process: 1
	queue length: 32
	throughput: 93466.2 ops/s (30000 executions in 320.972 ms)
Test running...(pid: 11992)

kernel execution throughput:
	process(es): 1
	thread(s) per process: 2
	queue length: 32
	throughput: 96251.4 ops/s (60000 executions in 623.367 ms)
Test running...(pid: 11992)

kernel execution throughput:
	process(es): 1
	thread(s) per process: 4
	queue length: 32
	throughput: 97069.1 ops/s (120000 executions in 1236.23 ms)
Test running...(pid: 11992)

kernel execution throughput:
	process(es): 1
	thread(s) per process: 8
	queue length: 32
	throughput: 97293.6 ops/s (240000 executions in 2466.76 ms)

```
### kernel execution latency test
```
./host.exe -k /opt/xilinx/dsa/xilinx_u200_xdma_201830_2/test/verify.xclbin -b 1 -L
Test running...(pid: 12029)

kernel execution latency:
	process(es): 1
	thread(s) per process: 1
	queue length: 1
	count: 30000
	min: 0.035923 ms
	max: 0.194254 ms
	avg: 0.050261 ms
```
### dma bandwidth test with 64 64M size BO Sync 
```
./host.exe -k /opt/xilinx/dsa/xilinx_u250_xdma_201830_3/test/verify.xclbin -K dma -s 64m -n 64
Test running...(pid: 12254)

DMA FPGA read throughput:
	process(es): 1
	thread(s) per process: 1
	bo size: 64m
	bandwidth: 9171.8 MB/s (64 transfers in 468.28 ms)
Test running...(pid: 12254)

DMA FPGA write throughput:
	process(es): 1
	thread(s) per process: 1
	bo size: 64m
	bandwidth: 11214.6 MB/s (64 transfers in 382.98 ms)
```
### dma latency test with 4K size BO(default size) Sync in 3 seconds by 2 threads 
```
./host.exe -k /opt/xilinx/dsa/xilinx_u250_xdma_201830_3/test/verify.xclbin -K dma -L -t 2 -T 3
Test running...(pid: 12339)

DMA FPGA read latency:
	process(es): 1
	thread(s) per process: 2
	bo size: 4k
	count: 266538
	min: 0.014877 ms
	max: 0.139194 ms
	avg: 0.019816 ms
Test running...(pid: 12339)

DMA FPGA write latency:
	process(es): 1
	thread(s) per process: 2
	bo size: 4k
	count: 272743
	min: 0.015086 ms
	max: 0.688914 ms
	avg: 0.019212 ms

```
### Single run with specified options combination 
kernel execution throughput with
    2 process(-p 2), 
    3 threads each(-t 3),
    1000 executions each thread(-n 1000),
    16 cmds sent before checking completion(-b 16), 
```
./host.exe -k /opt/xilinx/dsa/xilinx_u250_xdma_201830_3/test/verify.xclbin -t 3 -p 2 -n 1000 -b 16
Test running...(pid: 12507)
Test running...(pid: 12506)

multiple process kernel execution throughput:
	processes: 2
	thread(s) per process: 3
	cmd queue length: 16
	throughput: 46559.9 ops/s (6000 executions in 128.866 ms)

```
kernel execution latency (-L) with
    1 process, 
    2 threads each(-t 2),
    3 second(-T 3),
    ping-pong(-b 1),
 
```
./host.exe -k /opt/xilinx/dsa/xilinx_u250_xdma_201830_3/test/verify.xclbin -T3 -L -t 2 -b 1
Test running...(pid: 12529)

kernel execution latency:
	process(es): 1
	thread(s) per process: 2
	queue length: 1
	count: 57884
	min: 0.069562 ms
	max: 0.148273 ms
	avg: 0.092167 ms

```
## Build
```
$>make clean
$>make
```
