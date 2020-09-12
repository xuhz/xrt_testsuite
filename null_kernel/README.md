# xrt testsuite
## About
This test program can be used to run kernel execution throughput and latency test for NULL kernel.
NULL kernel is the kind of kernel does nothing but accept different number of arguments.

The test is xrt native API based, having similar overhead to the opencl API based
 
## cmdline: 
```
>./null_kernel.exe -k ../xclbin/null_kernel.xclbin 
Test running...(pid: 3202663, xclbin loaded in 5683.65 ms)
thread 0 running kernel name: null_a:{null_a_1}
num of args to kernel: 1

kernel execution throughput:
	process(es): 1
	thread(s) per process: 1
	queue length: 32
	throughput: 192982 ops/s (30000 executions in 155.455 ms)

>./null_kernel.exe -k ../xclbin/null_kernel.xclbin -N "null_p:{null_p_1}"
Test running...(pid: 3202800, xclbin loaded in 184.436 ms)
thread 0 running kernel name: null_p:{null_p_1}
num of args to kernel: 16

kernel execution throughput:
	process(es): 1
	thread(s) per process: 1
	queue length: 32
	throughput: 31436.6 ops/s (30000 executions in 954.301 ms)

```
## Build
```
$>make clean
$>make
```
