/**
 * Copyright (C) 2020 Xilinx, Inc
 * Author: Brian Xu(brianx@xilinx.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string.h>
#include <getopt.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cmath>
#include <ctime>
#include <time.h>
#include <chrono>
#include <thread>
#include <future>
#include <boost/algorithm/string.hpp>
#include "boost/filesystem.hpp"

#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"
#include "experimental/xrt_bo.h"

const std::string csv_history_file = "tput_history.csv";
const std::string qor_csv_file = "data_points.csv";
const std::string EXT = "xxxxoooo";
const std::string TMP = "tmpxxxxoooo/";
#define DEFAULT_COUNT (30000)
#define DEFAULT_BULK (32)
static size_t get_value(std::string& szStr);

struct Param {
    unsigned int device_index;
    int processes;
    int threads;
    int bulk;
    int loop;
    double time;
    bool latency;
    bool quiet;
    std::string& xclbin_file;
    int dir;
    std::string& bo_sz;
    int mode;
};

struct Count {
    long min;
    long max;
    long avg;
    size_t count;
};

struct MaxT {
    int processes;
    int threads;
    int bulk;
    double tput;
};

class Timer {
    double period;
public:
    std::chrono::high_resolution_clock::time_point start;
    std::chrono::high_resolution_clock::time_point end;
    Timer(double pd = 0) {
        start = std::chrono::high_resolution_clock::now();
        period = pd*1000;
    }
    void stop() {
        end = std::chrono::high_resolution_clock::now();
    }
    double elapsed() const {
        return std::chrono::duration<double, std::milli>(end- start).count();
    }
    bool expire() const {
        auto e = std::chrono::high_resolution_clock::now();
        return period && std::chrono::duration<double, std::milli>(e - start).count() > period;
    }
};

class Cmd {
public:    
    Cmd(const xrtDeviceHandle& device, const xrt::kernel& kernel, std::string& szStr,
       bool latency, int dir) :
       kernel(kernel), lat(latency), bosync((xclBOSyncDirection)dir)
    {
        auto sz = get_value(szStr);
        bo = xrt::bo(device, sz, 0, kernel.group_id(0));
        hptr = bo.map();
        bo_size = sz;
    }

    void run()
    {
        if (is_dma_test())
            run_dma_test();
        else
            run_kernel_test();
    }

    bool done()
    {
        if (is_dma_test())
            return true;
        else
            return kernel_done();
    }

    void wait()
    {
        if (!is_dma_test())
            kernel_wait();
    }

    Count count = {LLONG_MAX, LLONG_MIN, 0, 0};

private:
    xrt::kernel kernel;
    xrt::run cmd;
    bool lat;
    xclBOSyncDirection bosync;
    long stamp = 0;

    xrt::bo bo;
    void *hptr;
    size_t bo_size;

    bool is_dma_test()
    {
        return (bosync == XCL_BO_SYNC_BO_TO_DEVICE || bosync == XCL_BO_SYNC_BO_FROM_DEVICE);
    }

    void run_dma_test()
    {
        if (lat)
            stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        bo.sync(bosync, bo_size, 0);
        count.count++;
        if (lat) {
            auto end = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            update_lat(end);
        }
    }

    void run_kernel_test()
    {
        if (cmd)
            cmd.start();
        else
            cmd = kernel(bo);
        
        if (lat)
            stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    }

    bool kernel_done()
    {
        auto state = cmd.wait(1000);
        switch (state) {
            case ERT_CMD_STATE_COMPLETED:
            case ERT_CMD_STATE_ERROR:
            case ERT_CMD_STATE_ABORT:
                if (lat) {
                    auto end = std::chrono::high_resolution_clock::now().time_since_epoch().count();
                    update_lat(end);
                }
                count.count++;
                return true;
            default:
                break;
        }
        return false;
    }

    void kernel_wait()
    {
        cmd.wait();
    }

    void update_lat(long end)
    {
        auto delta = end - stamp;
        count.min = std::min(count.min, delta);
        count.max = std::max(count.max, delta);
        count.avg = (delta + count.count * count.avg) / (count.count + 1);
    }

};

static size_t get_value(std::string& szStr)
{
    char c = szStr.back();
    if (isdigit(c))
        return std::atol(szStr.c_str());

    size_t ret = 0;
    const char *v = szStr.substr(0, szStr.size() - 1).c_str();
    switch (c) {
        case 'b':
        case 'B':
            ret = std::atol(v);
            break;
        case 'k':
        case 'K':
            ret = 1024 * std::atol(v);
            break;
        case 'm':
        case 'M':
            ret = 1024 * 1024 * std::atol(v);
            break;
        case 'g':
        case 'G':
            ret = 1024 * 1024 * 1024 * std::atol(v);
            break;
        default:    
            throw std::runtime_error("-s input error!");
    }
    if (ret > 0x100000000)
            throw std::runtime_error("-s input too big!!");
    return ret;
}
void usage(char* exename)
{
    std::cout << "usage:\n";
    std::cout << "  " << exename << " [options] -k <bitstream>\n\n";
    std::cout << "  options:\n";
    std::cout << "\t-k <bitstream>, specifying path to xclbin file, mandatory \n";
    std::cout << "\t-b <bulk>, specifying cmd queue length per thread, optional,\n";
    std::cout << "\t           default is minimum of 32 and number of executions (see -n)\n";
    std::cout << "\t           cmd queue length number of cmds will be issued before polling cmd status,\n";
    std::cout << "\t           this is the aka bulk submit, then afterwards, a new cmd will be issued only after one cmd is complete\n";
    std::cout << "\t           when bulk size is 1, it is ping-pong test\n"; 
    std::cout << "\t-d <index>, specifying index to FPGA device, optional, default is 0\n";
    std::cout << "\t-n <count>, specifying number of kernel executions per thread, optional, defualt is 30000\n";
    std::cout << "\t-s <bo size>, specifying size of BO, optional, default is 4k\n";
    std::cout << "\t-t <threads>, specifying number of threads per process, optional, default is 1\n";
    std::cout << "\t-p <processes>, specifying number of processes spawned, optional, default is 1\n";
    std::cout << "\t-T <second>, specifying number of second the test will run, exclusive to -n, optional\n";
    std::cout << "\t-L if specified, will test latency\n";
    std::cout << "\t-m <mode>, optional, default is 0\n";
    std::cout << "\t           0:      single run with specified -b, -n | -T, -t, -p, -L\n";
    std::cout << "\t           1|tput: throughput test, run with different bulk size from 1 ,2, 4, all the way up to 256\n"; 
    std::cout << "\t                   only 1 process will be used in this case\n"; 
    std::cout << "\t           2|mp:   multiple process test, run with different processes from 1 to the next of power of 2 of specified\n"; 
    std::cout << "\t                     eg. -p 4, will run 1, 2, 4 processes\n"; 
    std::cout << "\t                     eg. -p 9, will run 1, 2, 4, 8, 16 processes\n"; 
    std::cout << "\t           3|mt:   multiple thread test, run with different threads from 1 to the next of power of 2 of specified\n"; 
    std::cout << "\t                     eg. -t 4, will run 1, 2, 4 threads\n"; 
    std::cout << "\t                     eg. -t 9, will run 1, 2, 4, 8, 16 threads\n"; 
    std::cout << "\t           4|dma:  dma test, single run with specified -b, -n | -T, -t, -L, only 1 process will be used\n";
    std::cout << "\t-h, help\n\n";
}

static void saveProcessResult(const Timer& timer, const Count& res)
{
    std::string file = TMP;
    file += std::to_string(getppid());
    file += "_";
    file += std::to_string(getpid());
    file += EXT;
    if (!boost::filesystem::exists(TMP))
        boost::filesystem::create_directory(TMP);
    std::ofstream handle(file);
    handle << timer.start.time_since_epoch().count() << "\n";
    handle << timer.end.time_since_epoch().count() << "\n";
    handle << res.min << "\n";
    handle << res.max << "\n";
    handle << res.avg << "\n";
    handle << res.count << "\n";
    handle.close();
}

static void printResult(const Param& param, const Timer& timer, const std::vector<std::vector<Cmd>>& cmds, MaxT& maxT)
{
    Count res = {LLONG_MAX, LLONG_MIN, 0, 0};
    for (auto& t : cmds) {
        size_t c = 0;
        for (auto& t1: t) {
            c += t1.count.count;
            if (param.latency) {
                /*
                std::cout << "min: " << t1.count.min;
                std::cout << " max " << t1.count.max;
                std::cout << " avg: " << t1.count.avg;
                std::cout << " count: " << t1.count.count;
                std::cout << "\n";
                */
                res.min = std::min(res.min, t1.count.min);
                res.max = std::max(res.max, t1.count.max);
                res.avg = (res.avg * res.count + t1.count.avg * t1.count.count) / (res.count + t1.count.count);
            }
            res.count += t1.count.count;
        }
        if (!param.time && c != (size_t)param.loop)
            throw std::runtime_error("per thread count calculation error");
    }
    if (!param.quiet) {
        std::ofstream handle(qor_csv_file, std::ofstream::app);
        handle << "{";
        handle << "\"metric\":";
        if (param.mode == 3) {
            handle << "\"multi thread ";
        } else {
            handle << "\"single run ";
        }
        if (param.dir == XCL_BO_SYNC_BO_TO_DEVICE) {
            std::cout << "\nDMA FPGA read ";
            handle << "DMA FPGA read ";
        } else if (param.dir == XCL_BO_SYNC_BO_FROM_DEVICE) {
            std::cout << "\nDMA FPGA write ";
            handle << "DMA FPGA write ";
        } else {
            std::cout << "\nkernel execution ";
            handle << "kernel execution ";
        }
        if (!param.latency) {
            std::cout << "throughput:\n";
            handle << "throughput\",";
        } else {
            std::cout << "latency:\n";
            handle << "latency\",";
        }
        std::cout <<  "\tprocess(es): " << param.processes << std::endl;
        handle << "\"process\":" << param.processes << ",";
        std::cout <<  "\tthread(s) per process: " << param.threads << std::endl;
        handle << "\"thread\":" << param.threads << ",";
        if (!param.latency) {
            if (param.dir == XCL_BO_SYNC_BO_TO_DEVICE ||
                param.dir == XCL_BO_SYNC_BO_FROM_DEVICE) {
                std::cout << "\tbo size: " << param.bo_sz << std::endl;
                handle << "\"bo size\":" << param.bo_sz << ",";
                std::cout << "\tbandwidth: ";
                handle << "\"bandwidth\":";
                std::cout << res.count * get_value(param.bo_sz) / timer.elapsed() / 1000 << " MB/s (";
                std::cout << res.count << " transfers in " << timer.elapsed() << " ms)\n";
                handle <<  res.count * get_value(param.bo_sz) / timer.elapsed() / 1000 << " MB/s"; 
            } else {
                std::cout << "\tqueue length: " << param.bulk << std::endl;
                handle << "\"queue length\":" << param.bulk << ",";
                std::cout << "\tthroughput: ";
                handle << "\"throughput\":";
                std::cout << res.count / timer.elapsed() * 1000 << " ops/s (";
                std::cout << res.count << " executions in " << timer.elapsed() << " ms)\n";
                handle << res.count / timer.elapsed() * 1000 << " ops/s";
            }
        } else {
            std::cout << "\tcount: " << res.count << std::endl;
            handle << "\"count\":" << res.count << ",";
            std::cout << "\tmin: " << (double)res.min / 1000000 << " ms\n";
            handle << "\"min\":" << (double)res.min / 1000000 << " ms,";
            std::cout << "\tmax: " << (double)res.max / 1000000 << " ms\n";
            handle << "\"max\":" << (double)res.max / 1000000 << " ms,";
            std::cout << "\tavg: " << (double)res.avg / 1000000 << " ms\n";
            handle << "\"avg\":" << (double)res.avg / 1000000 << " ms";
        }
        handle << "}\n";
        handle.close();
    }
    saveProcessResult(timer, res); // for multiple process
    MaxT nmaxT = {
        param.processes,
        param.threads,
        param.bulk,
        res.count * 1000 /timer.elapsed(),
    };
    if (nmaxT.tput > maxT.tput)
        maxT = std::move(nmaxT);
            
}

static void getHostname(char host[256])
{
    memset(host, 0, 256);
    if (gethostname(host, 256) == -1)
        throw std::runtime_error("gethostname");
}

/*
 * When running throughput test, the maximum tput is save on disk file with format
 *      test run on 'hostname' at 'date'
 *      xclbin: path_to_xclbin
 * path_to_xclbin should contain info of the shell 
 */ 
static void showTputResult(const Param& param, const MaxT& maxT)
{
    if (param.latency)
        return;
    std::ofstream handle(csv_history_file, std::ofstream::app);
    handle << "----------------------------------------------\n";
    auto t = std::chrono::system_clock::now();
    auto stamp = std::chrono::system_clock::to_time_t(t);
    char host[256];
    getHostname(host);
    handle << "test run on " << host << " at: " << std::ctime(&stamp);
    handle << "xclbin: " << param.xclbin_file << "\n";
    handle << "-----------------\n";
    std::cout << "\nMax throughput: " << maxT.tput << " ops/s\n";
    handle << "\nMax throughput: " << maxT.tput << " ops/s\n";
    std::cout << "@ processes: " << maxT.processes;
    handle << "processes: " << maxT.processes;
    std::cout << " / threads: " << maxT.threads;
    handle << " / threads: " << maxT.threads;
    std::cout << " / cmd queue length: " << maxT.bulk << std::endl;
    handle << " / cmd queue length: " << maxT.bulk << std::endl;
    handle << "-----------------\n";
    handle.close();
}

/*
 * For multiple process case, the overhead of setup and teardown of a process is not negligible,
 * we should not count the time as part of the time run. The way we are doing is, each child process
 * saves the start and end time and number of executions, the parent process gets the earliest start
 * and latest end as the period. This way is still not accurate.
 * Better way is the driver can provide number of executions with a tool, like custat.
 */   
static void handleProcessResult(const Param& param)
{
    double min = LLONG_MAX, max = LLONG_MIN;
    double count = 0, avg = 0, tcount, tavg;
    boost::filesystem::directory_iterator dir(TMP), end;
    while (dir != end) {
        std::string fn = dir->path().filename().string();
        if (fn.rfind(EXT) != std::string::npos) {
            //std::cout << "fn: " << fn << " pid: " << getpid() << std::endl;
            /*
             * contents save in file
             * 1 timer_start (throughput)
             * 2 timer_end (throughput) 
             * 3 lat_min (latency)
             * 4 lat_max (latency)
             * 5 lat_avg (latency)
             * 6 count (throughput, latency)
             */  
            if (fn.find(std::to_string(getpid()) + "_") != std::string::npos) {
                std::ifstream f(TMP + fn);
                if (f.is_open()) {
                    std::string ret;
                    std::getline(f, ret); // 1
                    if (!param.latency)
                        min = std::min(min, std::atof(ret.c_str()));	
                    std::getline(f, ret); // 2
                    if (!param.latency)
                        max = std::max(max, std::atof(ret.c_str()));	
                    std::getline(f, ret); // 3
                    if (param.latency)
                        min = std::atof(ret.c_str());
                    std::getline(f, ret); // 4
                    if (param.latency)
                        max = std::atof(ret.c_str());
                    std::getline(f, ret); // 5
                    if (param.latency)
                        tavg = std::atof(ret.c_str());
                    std::getline(f, ret); // 6
                    tcount = std::atof(ret.c_str());
                    if (param.latency)
                        avg = (avg * count + tavg * tcount) / (tcount + count);	
                    count += tcount;
                    f.close();
                }
            }
            boost::filesystem::remove_all(dir->path());
        }
        dir++;
    }
    if (boost::filesystem::exists(TMP))
        boost::filesystem::remove_all(TMP);

    std::ofstream handle(qor_csv_file, std::ofstream::app);
    handle << "{";
    handle << "\"metric\":";
    std::cout << "\nmultiple process kernel execution ";
    handle << "\"multiple process kernel execution ";
    if (param.latency) {
        std::cout << "latency:\n";
        handle << "latency\",";
    } else {
        std::cout << "throughput:\n";
        handle << "throughput\",";
    }
    std::cout << "\tprocesses: " << param.processes << std::endl;
    handle << "\"process\":" << param.processes << ",";
    std::cout << "\tthread(s) per process: " << param.threads << std::endl;
    handle << "\"thread\":" << param.threads << ",";
    std::cout << "\tcmd queue length: " << param.bulk << std::endl;
    handle << "\"cmd queue length\":" << param.bulk << ",";
    if (param.latency) {
        std::cout << "\tcount: " << count << std::endl;
        handle << "\"count\":" << count << ",";
        std::cout << "\tmin: " << min/1000000 << " ms\n";
        handle << "\"min\": " << min/1000000 << " ms,";
        std::cout << "\tmax: " << max/1000000 << " ms\n";
        handle << "\"max\": " << max/1000000 << " ms,";
        std::cout << "\tavg: " << avg/1000000 << " ms\n";
        handle << "\"avg\": " << avg/1000000 << " ms";
    } else {
        std::cout <<  "\tthroughput: " << count *1000000000 / (max - min) << " ops/s (";
        std::cout << count << " executions in " << (max - min)/1000000 << " ms)\n";
        handle << "\"throughput\": " <<  count *1000000000 / (max - min) << " ops/s";
    }
    handle << "}\n";
    handle.close();
}

static int make_p2(int n)
{
    if (std::ceil(log2(n)) == std::floor(log2(n)))
        return n;
    else
        return std::pow(2, std::ceil(log2(n)));
}

static int get_mode(const char* str)
{
    if (!strcasecmp(str, "tput"))
        return 1;
    if (!strcasecmp(str, "mp"))
        return 2;
    if (!strcasecmp(str, "mt"))
        return 3;
    if (!strcasecmp(str, "dma"))
        return 4;
    return std::atoi(str);
}

static int
run_multiple_process(char **argv, char *envp[], const Param& param)
{
    pid_t pids[param.processes];
    int c, status;

    for (c = 0; c < param.processes; c++) {
        status = posix_spawn(&pids[c], argv[0], NULL, NULL, argv, envp);
        if (status)
            throw std::runtime_error("posix_spawn failed");
        //std::cout << "process: " << pids[c] << " spawned..." << std::endl;
    }

    for (c = 0; c < param.processes; c++) {
        if (waitpid(pids[c], &status, 0) == -1)
            throw std::runtime_error("waitpid failed");
        //std::cout << "process: " << pids[c] << " exited." << std::endl;
    }

    handleProcessResult(param);

    return 0;
}

/*
 * cmds are pre-filled, so overhead of the cu param setup is not counted.
 * when running, all the cmds in the queue will be sent to the cu before starting
 * to check the cmd status. then once a cmd is complete, the same cmd will be issued
 * again.
 * a loop is used to check all the cmds one by one -- this is not an efficient way though
 */ 
static void
thr0(std::vector<Cmd>& cmds, int loop, const Timer& timer)
{
    int issued = 0, completed = 0;
    uint32_t c = 0;
    for (auto& cmd : cmds) {
        cmd.run();
        issued++;
    }

    c = 0;
    while (!loop || completed < loop) {
        if (cmds[c].done()) {
                completed++;
                if (!loop || issued < loop) {
                    cmds[c].run();
                    issued++;
                }
        }    

        if (++c == cmds.size())
            c = 0;
        if (!loop && timer.expire()) {
            break;
        }
    }
    /*
     * If time (-T) is spedified, after timer expires, still make sure all cmds complete.
     * but we don't count them.
     */  
    if (!loop) {
        for (auto& cmd : cmds) {
            cmd.wait(); 
        }
    }
}

static int run(const Param& param, MaxT& maxT)
{
    auto device = xrt::device(param.device_index);
    auto uuid = device.load_xclbin(param.xclbin_file);
    auto hello = xrt::kernel(device, uuid.get(), "hello:hello_1");
    int c; 
    int bulk = std::min(param.bulk, param.loop);
    std::cout << "Test running...(pid: " << getpid() <<")\n";
    std::vector<std::thread> thrs;

    /*
     * populate the cmd queue before hand for each thread.
     */  
    std::vector<std::vector<Cmd>> cmds;
    for (c = 0; c < param.threads; c++) {
    	std::vector<Cmd> cmdlist;
    	for (int i = 0; i < bulk; i++) {
        	auto cmd = Cmd(device, hello, param.bo_sz, param.latency, param.dir);
        	cmdlist.push_back(std::move(cmd));
    	}
       	cmds.push_back(std::move(cmdlist));
    }

    /*
     * For multiple threads case, the time of the thread setup and tear down is also
     * counted as the time running the kernel. This impact the accuracy.
     * Better way is, driver has statistics and can be reported by a tool like, custat
     */  
    Timer timer(param.time);
    if (param.threads == 1) {
        thr0(cmds[0], param.time ? 0 : param.loop, timer);
    } else {
        for (c = 0; c < param.threads; c++)
            thrs.emplace_back(&thr0, std::ref(cmds[c]), param.time ? 0 : param.loop, std::ref(timer));
        for (auto& t : thrs)
            t.join();
    }
    timer.stop();

    printResult(param, timer, cmds, maxT);
    
    return 0;
}

int run(int argc, char** argv, char *envp[])
{
    std::string xclbin_fnm;
    bool quiet = false;
    bool lat = false;
    unsigned int device_index = 0;
    int loop = DEFAULT_COUNT;
    int threads = 1;
    int bulk = DEFAULT_BULK;
    int processes = 1;
    int c;
    int mode = 0;
    double time = 0;
    int dir = INT_MAX;
    std::string boStr = "4k";
    std::vector<char *> nargv;
    nargv.push_back(argv[0]);
    
    while ((c = getopt(argc, argv, "b:d:hk:m:n:p:qs:t:LT:")) != -1) {
        switch (c)
        {
        case 'b':
            bulk = std::atoi(optarg);
            nargv.push_back((char *)"-b");
            nargv.push_back(optarg);
            break;    
        case 'd':
            device_index = std::atoi(optarg);
            nargv.push_back((char *)"-d");
            nargv.push_back(optarg);
            break;
        case 'k':    
            xclbin_fnm = optarg;
            nargv.push_back((char *)"-k");
            nargv.push_back(optarg);
            break;
        case 'n':
            loop = std::atoi(optarg);
            nargv.push_back((char *)"-n");
            nargv.push_back(optarg);
            break;    
        case 's':
            boStr = optarg;
            nargv.push_back((char *)"-s");
            nargv.push_back(optarg);
            break;
        case 't':
            threads = std::atoi(optarg);
            nargv.push_back((char *)"-t");
            nargv.push_back(optarg);
            break;
        case 'T':
            time = std::atof(optarg);
            nargv.push_back((char *)"-T");
            nargv.push_back(optarg);    
            break;                      
        case 'p':                       
            processes = std::atoi(optarg);
            break;                      
        case 'm':                       
            mode = get_mode(optarg);
            break;                      
        case 'h':                       
            usage(argv[0]);             
            return 1;                   
        case 'q':                       
            quiet = true;               
            break;                      
        case 'L':                       
            lat = true;               
            nargv.push_back((char *)"-L");
            nargv.push_back((char *)"");
            break;                      
        default:                        
            usage(argv[0]);             
            throw std::runtime_error    ("Unknown option value");
        }                               
    }                                   
                                        
    if (xclbin_fnm.empty())
        throw std::runtime_error("FAILED_TEST\nNo xclbin specified");    
                                                                         
    if (device_index >= xclProbe())                                      
        throw std::runtime_error("Cannot find device index (" + std::to_string(device_index) + ") specified");
  
    Param param = {device_index, processes, threads, bulk, loop, time, lat, quiet, xclbin_fnm, dir, boStr, mode};
    MaxT maxT = {0};

    if (mode == 1) { /*throughput test. one 1 process is being used.*/
        std::cout << "\nThroughput test...\n";
        param.processes = 1;
        auto t = make_p2(param.threads);
        for (int i = 1; i <= t; i *= 2) {
            param.threads = i;
            for (int j = 1; j <= 256; j *= 2) {
                param.bulk = j;
                run(param, maxT);
            }
        }
        showTputResult(param, maxT);
    } else if (mode == 2) { /*multiple process test*/
        std::cout << "\nMultiple process test...\n";
        if (param.processes == 1) {
            std::cout << "Warning: -p to specify maximum processes!!!\n\n";
            param.processes = 8;
        }
        auto p = make_p2(param.processes);
        if (p != param.processes) {
            std::cout << "Roundup processes to " << p << "(next power of 2)\n";
        }
        nargv.push_back((char *)"-q");
        for (int i = 1; i <= p; i *= 2) {
            param.processes = i;
            run_multiple_process(nargv.data(), envp, param);
        }
    } else if (mode == 3) { /*multiple thread test*/
        std::cout << "\nMultiple thread test...\n";
        if (param.threads == 1) {
            std::cout << "Warning: -t to specify maximum threads!!!\n\n";
            param.threads = 8;
        }
        auto t = make_p2(param.threads);
        if (t != param.threads) {
            std::cout << "Roundup threads to " << t << "(next power of 2)\n";
        }
        for (int i = 1; i <= t; i *= 2) {
            param.threads = i;
            run(param, maxT);
        }
    } else if (mode == 4) { /*dma test*/
        std::cout << "\nDMA test...\n";
        param.processes = 1;
        if (param.loop == DEFAULT_COUNT) {
            auto sz = get_value(param.bo_sz);
            if (sz > 0x40000000) //1G
                param.loop = 1;
            else if (sz > 0x10000000) //256M
                param.loop = 16;
            else if (sz > 0x1000000) //16M
                param.loop = 64;
            else if (sz > 0x100000) //1M
                param.loop = 4096;
            else
                param.loop = 10000;
        }
        param.dir = XCL_BO_SYNC_BO_TO_DEVICE;
        run(param, maxT);
        param.dir = XCL_BO_SYNC_BO_FROM_DEVICE;
        run(param, maxT);
    } else {
        if (processes > 1) {
            /*
             * when running multiple process test, we don't print number for each process/thread,
             * just print the whole instead.
             */  
            nargv.push_back((char *)"-q");
            return run_multiple_process(nargv.data(), envp, param);
        }

        run(param, maxT);
    }
    return 0;
}

int main(int argc, char** argv, char *envp[])
{
    try {
        return run(argc, argv, envp);
    }
    catch (std::exception const& e) {
        std::cout << "Exception: " << e.what() << "\n";
        std::cout << "Test failed(pid: " << getpid() <<")!!\n";
        return 1;
    }
    
    return 0;
}
