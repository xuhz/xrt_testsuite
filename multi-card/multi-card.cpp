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
#include <mutex>
#include <future>
#include <boost/algorithm/string.hpp>
#include "boost/filesystem.hpp"

#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"
#include "experimental/xrt_bo.h"

std::mutex print_mutex;
const std::string csv_history_file = "tput_history.csv";
const std::string qor_csv_file = "data_points.csv";
const std::string EXT = "xxxxoooo";
const std::string TMP = "tmpxxxxoooo/";
#define DEFAULT_COUNT (30000)
#define DEFAULT_BULK (32)
const std::string DEF_KNAME = "hello";

enum kernel_cu {
    KERNEL_CU_ILLEGAL = 0,
    MULTI_CU_PER_KERNEL = 1,
    MULTI_KERNEL_WITH_ONE_CU_EACH = 2,
    ONE_KERNEL_ONE_CU = 3,
};

enum kernel_run_type {
    RUN_TYPE_ILLEGAL = 0,
    RUN_TYPE_DMA = 1,
    RUN_TYPE_KERNEL = 2,
};

enum run_mode {
    MODE_ILLEGAL = 0,
    MODE_TPUT = 1,
    MODE_MP = 2,
    MODE_MT = 3,
    MODE_SINGLE_RUN = 4,
};

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
    std::string xclbin_file;
    int dir;
    std::string& bo_sz;
    int mode;
    int run_type;
    std::string& kname;
    int cu_type;
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
        std::chrono::milliseconds ts(1000);
        auto state = cmd.wait(ts);
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
    std::cout << "\t-k <bitstream>, specifying list of path to xclbin file, mandatory \n";
    std::cout << "\t           names of the files are separated by \",\"\n";
    std::cout << "\t-b <bulk>, specifying cmd queue length per thread, optional,\n";
    std::cout << "\t           default is minimum of 32 and number of executions (see -n)\n";
    std::cout << "\t           cmd queue length number of cmds will be issued before polling cmd status,\n";
    std::cout << "\t           this is the aka bulk submit, then afterwards, a new cmd will be issued only after one cmd is complete\n";
    std::cout << "\t           when bulk size is 1, it is ping-pong test\n"; 
    std::cout << "\t-d <index>, specifying list of index to FPGA device, mandatory\n";
    std::cout << "\t           names of the index are separated by \",\", order should match xclbin file order specified by -k\n";
    std::cout << "\t-n <count>, specifying number of kernel executions per thread, optional, defualt is 30000\n";
    std::cout << "\t-s <bo size>, specifying size of BO, optional, default is 4k\n";
    std::cout << "\t-t <threads>, specifying number of threads per process, optional, default is 1\n";
    std::cout << "\t-T <second>, specifying number of second the test will run, exclusive to -n, optional\n";
    std::cout << "\t-L if specified, will test latency\n";
    std::cout << "\t-K <run type> optional, default is 2\n";
    std::cout << "\t           1|dma: dma test\n";
    std::cout << "\t           2|kernel: kernel execution test\n";
    std::cout << "\t-N <kernel/cu name> optional,\n";
    std::cout << "\t            default for one cu per kernel is \"hello:{hello_1}\"\n";
    std::cout << "\t            default for multiple cu per kernel is \"hello_1:{hello_1_1}\"\n";
    std::cout << "\t-D <dma dir> 0: to device, 1: from device. optional, default is bi-direction\n";
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
    const std::lock_guard<std::mutex> lock(print_mutex);
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
        std::string line;
        line += "multi card ";
        if (param.mode == MODE_MT) {
            line += "multi thread ";
        }
        if (param.dir == XCL_BO_SYNC_BO_TO_DEVICE) {
            std::cout << "\nDMA FPGA read ";
            line += "DMA FPGA read\n";
        } else if (param.dir == XCL_BO_SYNC_BO_FROM_DEVICE) {
            std::cout << "\nDMA FPGA write ";
            line += "DMA FPGA write\n";
        } else {
            std::cout << "\nkernel execution ";
            line += "kernel execution\n";
        }
        if (!param.latency) {
            std::cout << "throughput:\n";
            line += "throughput\n";
        } else {
            std::cout << "latency:\n";
            line += "latency\n";
        }
	line += "{";
        std::cout <<  "\tdevice index: " << param.device_index << std::endl;
        line += "\"device_index\":" + std::to_string(param.device_index) + ",";
        std::cout <<  "\tprocess(es): " << param.processes << std::endl;
        line += "\"process\":" + std::to_string(param.processes) + ",";
        std::cout <<  "\tthread(s) per process: " << param.threads << std::endl;
        line += "\"thread\":" + std::to_string(param.threads) + ",";
        if (!param.latency) {
            if (param.dir == XCL_BO_SYNC_BO_TO_DEVICE ||
                param.dir == XCL_BO_SYNC_BO_FROM_DEVICE) {
                std::cout << "\tbo size: " << param.bo_sz << std::endl;
                line += "\"bo size\":" + param.bo_sz + ",";
                std::cout << "\tbandwidth: ";
                line += "\"bandwidth MB/s\":";
                std::cout << res.count * get_value(param.bo_sz) / timer.elapsed() / 1000 << " MB/s (";
                std::cout << res.count << " transfers in " << timer.elapsed() << " ms)\n";
                line +=  std::to_string(res.count * get_value(param.bo_sz) / timer.elapsed() / 1000); 
            } else {
                std::cout << "\tqueue length: " << param.bulk << std::endl;
                line += "\"queue length\":" + std::to_string(param.bulk) + ",";
                std::cout << "\tthroughput: ";
                line += "\"throughput ops/s\":";
                std::cout << res.count / timer.elapsed() * 1000 << " ops/s (";
                std::cout << res.count << " executions in " << timer.elapsed() << " ms)\n";
                line += std::to_string(res.count / timer.elapsed() * 1000);
            }
        } else {
            if (param.dir == XCL_BO_SYNC_BO_TO_DEVICE ||
                param.dir == XCL_BO_SYNC_BO_FROM_DEVICE) {
                std::cout << "\tbo size: " << param.bo_sz << std::endl;
                line += "\"bo size\":" + param.bo_sz + ",";
            } else {
                std::cout << "\tqueue length: " << param.bulk << std::endl;
                line += "\"queue length\":" + std::to_string(param.bulk) + ",";
            }
            std::cout << "\tcount: " << res.count << std::endl;
            line += "\"count\":" + std::to_string(res.count) + ",";
            std::cout << "\tmin: " << (double)res.min / 1000000 << " ms\n";
            handle << line + "\"min ms\":" + std::to_string((double)res.min / 1000000) + "}\n";
            std::cout << "\tmax: " << (double)res.max / 1000000 << " ms\n";
            handle << line + "\"max ms\":" + std::to_string((double)res.max / 1000000) + "}\n";
            std::cout << "\tavg: " << (double)res.avg / 1000000 << " ms\n";
            line += "\"avg ms\":" + std::to_string((double)res.avg / 1000000);
        }
        line += "}\n";
        handle << line;
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
    std::string line;
    line += "multiple process ";
    if (param.dir == XCL_BO_SYNC_BO_TO_DEVICE) {
        std::cout << "\nDMA FPGA read ";
        line += "DMA FPGA read\n";
    } else if (param.dir == XCL_BO_SYNC_BO_FROM_DEVICE) {
        std::cout << "\nDMA FPGA write ";
        line += "DMA FPGA write\n";
    } else {
        std::cout << "\nkernel execution ";
        line += "kernel execution\n";
    }
    if (!param.latency) {
        std::cout << "throughput:\n";
        line += "throughput\n";
    } else {
        std::cout << "latency:\n";
        line += "latency\n";
    }
    line += "{";
    std::cout <<  "\tprocess(es): " << param.processes << std::endl;
    line += "\"process\":" + std::to_string(param.processes) + ",";
    std::cout <<  "\tthread(s) per process: " << param.threads << std::endl;
    line += "\"thread\":" + std::to_string(param.threads) + ",";
    if (!param.latency) {
        if (param.dir == XCL_BO_SYNC_BO_TO_DEVICE ||
            param.dir == XCL_BO_SYNC_BO_FROM_DEVICE) {
            std::cout << "\tbo size: " << param.bo_sz << std::endl;
            line += "\"bo size\":" + param.bo_sz + ",";
            std::cout << "\tbandwidth: ";
            line += "\"bandwidth MB/s\":";
            std::cout << count * get_value(param.bo_sz) / (max - min) * 1000 << " MB/s (";
            std::cout << count << " transfers in " << (max - min) / 1000000 << " ms)\n";
            line +=  std::to_string(count * get_value(param.bo_sz) / (max - min) * 1000); 
        } else {
            std::cout << "\tqueue length: " << param.bulk << std::endl;
            line += "\"queue length\":" + std::to_string(param.bulk) + ",";
            std::cout <<  "\tthroughput: " << count *1000000000 / (max - min) << " ops/s (";
            std::cout << count << " executions in " << (max - min)/1000000 << " ms)\n";
            line += "\"throughput ops/s\": " + std::to_string(count *1000000000 / (max - min));
        }
    } else {
        if (param.dir == XCL_BO_SYNC_BO_TO_DEVICE ||
            param.dir == XCL_BO_SYNC_BO_FROM_DEVICE) {
            std::cout << "\tbo size: " << param.bo_sz << std::endl;
            line += "\"bo size\":" + param.bo_sz + ",";
        } else {
            std::cout << "\tqueue length: " << param.bulk << std::endl;
            line += "\"queue length\":" + std::to_string(param.bulk) + ",";
        }
        std::cout << "\tcount: " << count << std::endl;
        line += "\"count\":" + std::to_string(count) + ",";
        std::cout << "\tmin: " << (double)min / 1000000 << " ms\n";
        handle << line + "\"min ms\":" + std::to_string((double)min / 1000000) + "}\n";
        std::cout << "\tmax: " << (double)max / 1000000 << " ms\n";
        handle << line + "\"max ms\":" + std::to_string((double)max / 1000000) + "}\n";
        std::cout << "\tavg: " << (double)avg / 1000000 << " ms\n";
        line += "\"avg ms\":" + std::to_string((double)avg / 1000000);
    }
    line += "}\n";
    handle << line;
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
        return MODE_TPUT;
    if (!strcasecmp(str, "mp"))
        return MODE_MP;
    if (!strcasecmp(str, "mt"))
        return MODE_MT;
    return std::atoi(str);
}

static int get_run_type(const char* str)
{
    if (!strcasecmp(str, "dma"))
        return RUN_TYPE_DMA;
    if (!strcasecmp(str, "kernel"))
        return RUN_TYPE_KERNEL;
    return std::atoi(str);
}

static int get_cu_type(const char* str)
{
    if (!strcasecmp(str, "mc"))
        return MULTI_CU_PER_KERNEL;
    if (!strcasecmp(str, "mk"))
        return MULTI_KERNEL_WITH_ONE_CU_EACH;
    return std::atoi(str);
}

static void regulate_dma_run_param(Param& param, bool force = false)
{
    //param.processes = 1;
    if (force || param.loop == DEFAULT_COUNT) {
        auto sz = get_value(param.bo_sz);
        if (sz > 0x40000000) //1G
            param.loop = 1;
        else if (sz > 0x10000000) //256M
            param.loop = 4;
        else if (sz > 0x4000000) //64M
            param.loop = 16;
        else if (sz > 0x1000000) //16M
            param.loop = 64;
        else if (sz > 0x100000) //1M
            param.loop = 1024;
        else
            param.loop = 10000;
    }
}

static int
run_multiple_process(std::vector<char*>& argv, char *envp[], const Param& param,
    const std::vector<std::string>& fxclbin, const std::vector<std::string>& indexs)
{
    pid_t pids[param.processes];
    int c, status;

    for (c = 0; c < param.processes; c++) {
        argv.push_back((char *)"-k");
        std::string f = fxclbin[c];
        argv.push_back(&f[0]);
        argv.push_back((char *)"-d");
        std::string i = indexs[c];
        argv.push_back(&i[0]);
        status = posix_spawn(&pids[c], argv.data()[0], NULL, NULL, argv.data(), envp);
        argv.pop_back();
        argv.pop_back();
        argv.pop_back();
        argv.pop_back();
        if (status) {
            std::cerr << "status: " << status << std::endl;
            throw std::runtime_error("posix_spawn failed");
        }
        //std::cout << "process: " << pids[c] << " spawned..." << std::endl;
    }

    for (c = 0; c < param.processes; c++) {
        if (waitpid(pids[c], &status, 0) == -1)
            throw std::runtime_error("waitpid failed");
        //std::cout << "process: " << pids[c] << " exited." << std::endl;
    }

    //handleProcessResult(param);

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
    Timer timer_ld;
    auto uuid = device.load_xclbin(param.xclbin_file);
    timer_ld.stop();
    auto krnl = xrt::kernel(device, uuid.get(), param.kname, false);
    int c; 
    int bulk = std::min(param.bulk, param.loop);
    std::cout << "Test running...(pid: " << getpid() <<", xclbin loaded in " << timer_ld.elapsed() << " ms)\n";
    std::vector<std::thread> thrs;

    /*
     * populate the cmd queue before hand for each thread.
     */  
    std::vector<std::vector<Cmd>> cmds;
    for (c = 0; c < param.threads; c++) {
    	std::vector<Cmd> cmdlist;
    	for (int i = 0; i < bulk; i++) {
        	auto cmd = Cmd(device, krnl, param.bo_sz, param.latency, param.dir);
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

static void
check_param(const Param& param)
{
    if (param.xclbin_file.empty())
        throw std::runtime_error("\nNo -k specified");
                                                                         
    if (param.device_index >= xclProbe())                                      
        throw std::runtime_error("\n-d specified error");

    if (param.mode < MODE_TPUT || param.mode > MODE_SINGLE_RUN)
        throw std::runtime_error("\n-m specified error");

    if (param.run_type < RUN_TYPE_DMA || param.run_type > RUN_TYPE_KERNEL)
        throw std::runtime_error("\n-K specified error");

    if (param.dir < 0 || (param.dir > 1 && param.dir != INT_MAX))
        throw std::runtime_error("\n-D specified error");

    if (param.processes == 0)
        throw std::runtime_error("\n-p specified error");

    if (param.threads == 0)
        throw std::runtime_error("\n-t specified error");

    if (param.bulk == 0)
        throw std::runtime_error("\n-b specified error");

    if (param.loop == 0)
        throw std::runtime_error("\n-n specified error");

    if (param.cu_type == KERNEL_CU_ILLEGAL)
        throw std::runtime_error("\n-c specified error");
    else if (param.cu_type == MULTI_KERNEL_WITH_ONE_CU_EACH)
        param.kname = DEF_KNAME + "_1:{" + DEF_KNAME + "_1_1}";
}

static void
split(const std::string& input, std::vector<std::string>& output)
{
    size_t start = 0, last = 0;
    while ((start = input.find(",", last)) != std::string::npos) {
        output.push_back(input.substr(last, start));
        last = start+1;
    }
    output.push_back(input.substr(last));
}

int run(int argc, char** argv, char *envp[])
{
    std::string xclbin_fnm;
    bool quiet = false;
    bool lat = false;
    std::string device_index = "0";
    int loop = DEFAULT_COUNT;
    int threads = 1;
    int bulk = DEFAULT_BULK;
    int processes = 1;
    int c;
    int mode = MODE_SINGLE_RUN;
    int run_type = RUN_TYPE_KERNEL;
    int cu_type = ONE_KERNEL_ONE_CU;
    double time = 0;
    int dir = INT_MAX;
    std::string boStr = "4k";
    std::string kname = DEF_KNAME + ":{" + DEF_KNAME + "_1}";
    std::vector<char *> nargv;
    nargv.reserve(30);
    nargv.push_back(argv[0]);
    
    while ((c = getopt(argc, argv, "b:d:hk:n:qs:t:D:LK:T:N:")) != -1) {
        switch (c)
        {
        case 'b':
            bulk = std::atoi(optarg);
            nargv.push_back((char *)"-b");
            nargv.push_back(optarg);
            break;    
        case 'd':
            device_index = optarg;
            break;
        case 'k':    
            xclbin_fnm = optarg;
            break;
        case 'n':
            loop = std::atoi(optarg);
            nargv.push_back((char *)"-n");
            nargv.push_back(optarg);
            break;    
        case 'N':
            kname = optarg;
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
        case 'K':
            run_type = get_run_type(optarg);
            nargv.push_back((char *)"-K");
            nargv.push_back(optarg);    
            break;                      
        case 'T':
            time = std::atof(optarg);
            nargv.push_back((char *)"-T");
            nargv.push_back(optarg);    
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
        case 'D':
            dir = std::atoi(optarg);
            nargv.push_back((char *)"-D");
            nargv.push_back(optarg);    
            break;                      
        default:                        
            usage(argv[0]);             
            throw std::runtime_error("Unknown option value");
        }                               
    }                                   
 
    std::vector<std::string> fxclbin;
    std::vector<std::string> indexs;
    split(xclbin_fnm, fxclbin);
    split(device_index, indexs);
    processes = fxclbin.size();
    Param param = {0, processes, threads, bulk, loop, time, lat,
        quiet, "", dir, boStr, mode, run_type, kname, cu_type};
    MaxT maxT = {0};

    if (processes > 1) {
        /*
         * when running multiple process test, we don't print number for each process/thread,
         * just print the whole instead.
         */  
        //nargv.push_back((char *)"-q");
        if (param.dir == INT_MAX && param.run_type == RUN_TYPE_DMA) {
            param.dir = XCL_BO_SYNC_BO_TO_DEVICE;
            nargv.push_back((char *)"-D");
            nargv.push_back((char *)"0");
            run_multiple_process(nargv, envp, param, fxclbin, indexs);
            nargv.pop_back();
            nargv.push_back((char *)"1");
            param.dir = XCL_BO_SYNC_BO_FROM_DEVICE;
            run_multiple_process(nargv, envp, param, fxclbin, indexs);
        } else {
            run_multiple_process(nargv, envp, param, fxclbin, indexs);
        }
        return 0;
    }

    param.device_index = std::atoi(device_index.c_str());
    param.xclbin_file = xclbin_fnm;
    check_param(param);                     
    if (run_type == RUN_TYPE_KERNEL) {
        run(param, maxT);
    } else {
        regulate_dma_run_param(param);
        if (param.dir == INT_MAX) {
            param.dir = XCL_BO_SYNC_BO_TO_DEVICE;
            run(param, maxT);
            param.dir = XCL_BO_SYNC_BO_FROM_DEVICE;
            run(param, maxT);
        } else {
            run(param, maxT);
        }
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
