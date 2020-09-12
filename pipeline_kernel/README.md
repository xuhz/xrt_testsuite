# xrt testsuite
## About
This test program can be used to run kernel execution throughput and latency test for pipeline kernels.
pipeline kernels means first kernel and last kernel are Memory Mapped kernels which accept BO as input,
all other kernels in the middle are streaming kernels which run themselves without manupilation from host  

The test is xrt native API based, having similar overhead to the opencl API based
 
## cmdline: 
```
>./pipeline.exe -k ../xclbin/m2s2s2m.xclbin 
Test running...(pid: 3203883, xclbin loaded in 5646.76 ms)

kernel execution throughput:
	process(es): 1
	thread(s) per process: 1
	queue length: 32
	throughput: 58632.7 ops/s (30000 executions in 511.66 ms)

```
## Build
```
$>make clean
$>make
```
