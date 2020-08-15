# xrt testsuite
## About
This test program can be used to run 

* Throughput test
* Multiple thead test
* Multiple process test 
* Single run with specified number of process, thread, executions or period of time, and cmd queue
* length

The test is xrt native API based, having similar overhead to the opencl API based
 
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
	-L if specified, will test latency
	-m <mode>, optional, default is 0
	           0:      single run with specified -b, -n | -T, -t, -p, -L
	           1|tput: throughput test, run with different bulk size from 1 ,2, 4, all the way up to 256
	                   only 1 process will be used in this case
	           2|mp:   multiple process test, run with different processes from 1 to the next of power of 2 of specified
	                     eg. -p 4, will run 1, 2, 4 processes
	                     eg. -p 9, will run 1, 2, 4, 8, 16 processes
	           3|mt:   multiple thread test, run with different threads from 1 to the next of power of 2 of specified
	                     eg. -t 4, will run 1, 2, 4 threads
	                     eg. -t 9, will run 1, 2, 4, 8, 16 threads
	           4|dma:  dma test, single run with specified -b, -n | -T, -t, -L, only 1 process will be used
	-h, help
```
## Examples: 

### default run -- 1 process, 1 thread, cmd queue length 32, number of executions 30000
```
$>./host.exe -k /opt/xilinx/dsa/xilinx_u250_xdma_201830_3/test/verify.xclbin 
Test running...(pid: 6093)

kernel execution(cmd queue length: 32):
	process(es): 1
	thread(s) per process: 1
	throughput: 47242.9 ops/s (30000 executions in 635.016 ms)
```
### ping-pong test
```
$>./host.exe -k /opt/xilinx/dsa/xilinx_u250_xdma_201830_3/test/verify.xclbin -b 1
Test running...(pid: 6151)

kernel execution(cmd queue length: 1):
	process(es): 1
	thread(s) per process: 1
	throughput: 10594.4 ops/s (30000 executions in 2831.7 ms)

```
### run test on device 1, with 2 threads, for 5.1s
```
$>./host.exe -k /opt/xilinx/dsa/xilinx_u250_xdma_201830_3/test/verify.xclbin -t 2 -T 5.1 -d 1
Test running...(pid: 6129)

kernel execution(cmd queue length: 32):
	process(es): 1
	thread(s) per process: 2
	throughput: 46820.2 ops/s (238855 executions in 5101.53 ms)

```
### throughput test
```
$>./host.exe -k /opt/xilinx/dsa/xilinx_u250_xdma_201830_3/test/verify.xclbin -m 1

Throughput test...
Test running...(pid: 5978)

kernel execution(cmd queue length: 1):
	process(es): 1
	thread(s) per process: 1
	throughput: 10341.3 ops/s (30000 executions in 2900.99 ms)
Test running...(pid: 5978)

kernel execution(cmd queue length: 2):
	process(es): 1
	thread(s) per process: 1
	throughput: 19315.5 ops/s (30000 executions in 1553.15 ms)
Test running...(pid: 5978)

kernel execution(cmd queue length: 4):
	process(es): 1
	thread(s) per process: 1
	throughput: 23799.6 ops/s (30000 executions in 1260.52 ms)
Test running...(pid: 5978)

kernel execution(cmd queue length: 8):
	process(es): 1
	thread(s) per process: 1
	throughput: 30349.4 ops/s (30000 executions in 988.487 ms)
Test running...(pid: 5978)

kernel execution(cmd queue length: 16):
	process(es): 1
	thread(s) per process: 1
	throughput: 46938.3 ops/s (30000 executions in 639.137 ms)
Test running...(pid: 5978)

kernel execution(cmd queue length: 32):
	process(es): 1
	thread(s) per process: 1
	throughput: 47080.6 ops/s (30000 executions in 637.205 ms)
Test running...(pid: 5978)

kernel execution(cmd queue length: 64):
	process(es): 1
	thread(s) per process: 1
	throughput: 46824.6 ops/s (30000 executions in 640.688 ms)
Test running...(pid: 5978)

kernel execution(cmd queue length: 128):
	process(es): 1
	thread(s) per process: 1
	throughput: 46761.1 ops/s (30000 executions in 641.559 ms)
Test running...(pid: 5978)

kernel execution(cmd queue length: 256):
	process(es): 1
	thread(s) per process: 1
	throughput: 46748.3 ops/s (30000 executions in 641.735 ms)

Max throughput: 47080.6 ops/s
@ processes: 1 / threads: 1 / cmd queue length: 32

``` 

### multiple process test
```
$>./host.exe -k /opt/xilinx/dsa/xilinx_u250_xdma_201830_3/test/verify.xclbin -m mp -p 4

Multiple process test...
Test running...(pid: 6012)

kernel execution(cmd queue length: 32):
	process(es): 1
	thread(s) per process: 1
	throughput: 46765.4 ops/s (30000 executions in 641.5 ms)
Test running...(pid: 6014)
Test running...(pid: 6015)

kernel execution(cmd queue length: 32):
	process(es): 2
	thread(s) per process: 1
	throughput: 46811.6 ops/s (60000 executions in 1281.73 ms)
Test running...(pid: 6020)
Test running...(pid: 6021)
Test running...(pid: 6018)
Test running...(pid: 6019)

kernel execution(cmd queue length: 32):
	process(es): 4
	thread(s) per process: 1
	throughput: 46797 ops/s (120000 executions in 2564.27 ms)

```

### multiple thread test
```
$>./host.exe -k /opt/xilinx/dsa/xilinx_u250_xdma_201830_3/test/verify.xclbin -m 3 -t 5

Multiple thread test...
Roundup threads to 8(next power of 2)
Test running...(pid: 6042)

kernel execution(cmd queue length: 32):
	process(es): 1
	thread(s) per process: 1
	throughput: 46868.4 ops/s (30000 executions in 640.09 ms)
Test running...(pid: 6042)

kernel execution(cmd queue length: 32):
	process(es): 1
	thread(s) per process: 2
	throughput: 46794.1 ops/s (60000 executions in 1282.21 ms)
Test running...(pid: 6042)

kernel execution(cmd queue length: 32):
	process(es): 1
	thread(s) per process: 4
	throughput: 46868.8 ops/s (120000 executions in 2560.34 ms)
Test running...(pid: 6042)

kernel execution(cmd queue length: 32):
	process(es): 1
	thread(s) per process: 8
	throughput: 46799.7 ops/s (240000 executions in 5128.24 ms)

```
### latency test
```
$>./host.exe -k /opt/xilinx/dsa/xilinx_u250_xdma_201830_3/test/verify.xclbin -b 1 -L
Test running...(pid: 17095)

kernel execution(cmd queue length: 1):
	process(es): 1
	thread(s) per process: 1
	Kernel execution latency:
		min: 0.072146 ms
		max: 0.1672 ms
		avg: 0.093741 ms
		count: 30000

```
### dma bandwidth test
```
$>./host.exe -k /opt/xilinx/dsa/xilinx_u250_xdma_201830_3/test/verify.xclbin -K dma -s 64M -n 64

DMA test...
Test running...(pid: 17131)

DMA write(sz 64M):
	process(es): 1
	thread(s) per process: 1
	DMA write throughput(sz 64M): 10613.8 MB/s (64 transfers in 404.659 ms)
Test running...(pid: 17131)

DMA read(sz 64M):
	process(es): 1
	thread(s) per process: 1
	DMA read throughput(sz 64M): 10400.5 MB/s (64 transfers in 412.958 ms)
```
### dma latency test
```
$>./host.exe -k /opt/xilinx/dsa/xilinx_u250_xdma_201830_3/test/verify.xclbin -K dma -L -T 2 -t 2

DMA test...
Test running...(pid: 17149)

DMA write(sz 4k):
	process(es): 1
	thread(s) per process: 2
	DMA write latency(sz 4k):
		min: 0.016832 ms
		max: 0.68612 ms
		avg: 0.020242 ms
		count: 176440
Test running...(pid: 17149)

DMA read(sz 4k):
	process(es): 1
	thread(s) per process: 2
	DMA read latency(sz 4k):
		min: 0.016901 ms
		max: 0.127042 ms
		avg: 0.020229 ms
		count: 174844

```
## Build
```
$>make clean
$>make
```
