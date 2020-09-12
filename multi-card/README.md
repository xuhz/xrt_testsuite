# xrt testsuite
## About
This test program can be used to run multi card

* Kernel execution throughput and latency test
* DMA bandwidth and latency test
* Multiple thead test
* Single run with specified number of thread, executions or period of time, cmd queue
* length for kernel execution or bo size for DMA, throughput or latency 

The test is xrt native API based, having similar overhead to the opencl API based
 
## cmdline: 
```
$>./multi-card.exe -h
usage:
  ./multi-card.exe [options] -k <bitstream>

  options:
	-k <bitstream>, specifying list of path to xclbin file, mandatory 
	           names of the files are separated by 	-b <bulk>, specifying cmd queue length per thread, optional,
	           default is minimum of 32 and number of executions (see -n)
	           cmd queue length number of cmds will be issued before polling cmd status,
	           this is the aka bulk submit, then afterwards, a new cmd will be issued only after one cmd is complete
	           when bulk size is 1, it is ping-pong test
	-d <index>, specifying list of index to FPGA device, mandatory
	           names of the index are separated by 	-n <count>, specifying number of kernel executions per thread, optional, defualt is 30000
	-s <bo size>, specifying size of BO, optional, default is 4k
	-t <threads>, specifying number of threads per process, optional, default is 1
	-T <second>, specifying number of second the test will run, exclusive to -n, optional
	-L if specified, will test latency
	-K <run type> optional, default is 2
	           1|dma: dma test
	           2|kernel: kernel execution test
	-N <kernel/cu name> optional,
	            default for one cu per kernel is "hello:{hello_1}"
	            default for multiple cu per kernel is "hello_1:{hello_1_1}"
	-D <dma dir> 0: to device, 1: from device. optional, default is bi-direction
	-h, help

```
## Examples: 

### throughput test on device 0,1, for 5s 
```
>./multi-card.exe -k /opt/xilinx/dsa/xilinx_u200_xdma_201830_2/test/verify.xclbin,/opt/xilinx/dsa/xilinx_u250_xdma_201830_3/test/verify.xclbin -d 0,1 -T 5
Test running...(pid: 3199092, xclbin loaded in 214.228 ms)
Test running...(pid: 3199091, xclbin loaded in 158.583 ms)

kernel execution throughput:
	device index: 1
	process(es): 1
	thread(s) per process: 1
	queue length: 32
	throughput: 39267.5 ops/s (196374 executions in 5000.92 ms)

kernel execution throughput:
	device index: 0
	process(es): 1
	thread(s) per process: 1
	queue length: 32
	throughput: 98245.7 ops/s (491249 executions in 5000.21 ms)
``` 

### kernel execution latency test
```
>./multi-card.exe -k /opt/xilinx/dsa/xilinx_u200_xdma_201830_2/test/verify.xclbin,/opt/xilinx/dsa/xilinx_u250_xdma_201830_3/test/verify.xclbin -d 0,1 -L -n 1 -T 5
Test running...(pid: 3200107, xclbin loaded in 156.103 ms)
Test running...(pid: 3200108, xclbin loaded in 232.055 ms)

kernel execution latency:
	device index: 0
	process(es): 1
	thread(s) per process: 1
	queue length: 32
	count: 83875
	min: 0.000578 ms
	max: 9.31762 ms
	avg: 0.042941 ms

kernel execution latency:
	device index: 1
	process(es): 1
	thread(s) per process: 1
	queue length: 32
	count: 51401
	min: 0.055003 ms
	max: 12.3658 ms
	avg: 0.07854 ms

```
### dma bandwidth test with 32 64M size BO Sync 
```
>./multi-card.exe -k /opt/xilinx/dsa/xilinx_u200_xdma_201830_2/test/verify.xclbin,/opt/xilinx/dsa/xilinx_u250_xdma_201830_3/test/verify.xclbin -d 0,1 -Kdma -s 64m -n32
Test running...(pid: 3200674, xclbin loaded in 182.834 ms)
Test running...(pid: 3200675, xclbin loaded in 173.536 ms)

DMA FPGA read throughput:
	device index: 0
	process(es): 1
	thread(s) per process: 1
	bo size: 64m
	bandwidth: 9580.24 MB/s (32 transfers in 224.158 ms)

DMA FPGA read throughput:
	device index: 1
	process(es): 1
	thread(s) per process: 1
	bo size: 64m
	bandwidth: 9791.13 MB/s (32 transfers in 219.33 ms)
Test running...(pid: 3200679, xclbin loaded in 251.926 ms)
Test running...(pid: 3200678, xclbin loaded in 181.577 ms)

DMA FPGA write throughput:
	device index: 1
	process(es): 1
	thread(s) per process: 1
	bo size: 64m
	bandwidth: 10916.3 MB/s (32 transfers in 196.722 ms)

DMA FPGA write throughput:
	device index: 0
	process(es): 1
	thread(s) per process: 1
	bo size: 64m
	bandwidth: 10868.1 MB/s (32 transfers in 197.595 ms)

```
### dma latency test with 4K size BO(default size) Sync in 3 seconds by 2 threads 
```
>./multi-card.exe -k /opt/xilinx/dsa/xilinx_u200_xdma_201830_2/test/verify.xclbin,/opt/xilinx/dsa/xilinx_u250_xdma_201830_3/test/verify.xclbin -d 0,1 -Kdma -L -T 3 -t 2
Test running...(pid: 3201543, xclbin loaded in 176.635 ms)
Test running...(pid: 3201544, xclbin loaded in 232.266 ms)

DMA FPGA read latency:
	device index: 0
	process(es): 1
	thread(s) per process: 2
	bo size: 4k
	count: 306828
	min: 0.012112 ms
	max: 6.03878 ms
	avg: 0.017801 ms

DMA FPGA read latency:
	device index: 1
	process(es): 1
	thread(s) per process: 2
	bo size: 4k
	count: 307816
	min: 0.012756 ms
	max: 2.41691 ms
	avg: 0.017797 ms
Test running...(pid: 3201552, xclbin loaded in 223.131 ms)
Test running...(pid: 3201551, xclbin loaded in 208.73 ms)

DMA FPGA write latency:
	device index: 1
	process(es): 1
	thread(s) per process: 2
	bo size: 4k
	count: 346422
	min: 0.012009 ms
	max: 0.696524 ms
	avg: 0.015878 ms

DMA FPGA write latency:
	device index: 0
	process(es): 1
	thread(s) per process: 2
	bo size: 4k
	count: 347954
	min: 0.012702 ms
	max: 0.152464 ms
	avg: 0.015799 ms

```
## Build
```
$>make clean
$>make
```
