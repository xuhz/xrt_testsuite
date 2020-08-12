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

const std::string csv_file = "data_points.csv";
const std::string EXT = "xxxxoooo";
const std::string TMP = "tmpxxxxoooo/";
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
};

struct Count {
    bool lat;
    long min;
    long max;
    long avg;
    long count;
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

static void usage(char* exename)
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
    std::cout << "\t-t <threads>, specifying number of threads per process, optional, default is 1\n";
    std::cout << "\t-p <processes>, specifying number of processes spawned, optional, default is 1\n";
    std::cout << "\t-m <mode>, optional, default is 0\n";
    std::cout << "\t           0:      single run with specified -b, -n, -t, -p\n";
    std::cout << "\t           1|tput: throughput test, run with different bulk size from 1 ,2, 4, all the way up to 256\n"; 
    std::cout << "\t                   only 1 process will be used in this case\n"; 
    std::cout << "\t           2|mp:   multiple process test, run with different processes from 1 to the next of power of 2 of specified\n"; 
    std::cout << "\t                     eg. -p 4, will run 1, 2, 4 processes\n"; 
    std::cout << "\t                     eg. -p 9, will run 1, 2, 4, 8, 16 processes\n"; 
    std::cout << "\t           3|mt:   multiple thread test, run with different threads from 1 to the next of power of 2 of specified\n"; 
    std::cout << "\t                     eg. -t 4, will run 1, 2, 4 threads\n"; 
    std::cout << "\t                     eg. -t 9, will run 1, 2, 4, 8, 16 threads\n"; 
    std::cout << "\t           4|lat:   latency test, run with specified -b, -n, -t -p\n"; 
    std::cout << "\t-h, help\n\n";
}

static void saveProcessResult(const Count& count)
{
    std::string file = TMP;
    file += std::to_string(getppid());
    file += "_";
    file += std::to_string(getpid());
    file += EXT;
    if (!boost::filesystem::exists(TMP))
        boost::filesystem::create_directory(TMP);
    std::ofstream handle(file);
    handle << count.min << "\n";
    handle << count.max << "\n";
    handle << count.avg << "\n";
    handle << count.count << "\n";
    handle.close();
}

static void saveProcessResult(const Timer& timer, const double total)
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
    handle << total << "\n";
    handle.close();
}

static void printResult(const Param& param, const Count& count)
{
    if (1/*!param.quiet*/) {
        std::cout << "\nkernel execution(cmd queue length: " << param.bulk << "):\n";
        std::cout <<  "\tprocess(es): " << param.processes << std::endl;
        std::cout <<  "\tthread(s) per process: " << param.threads << std::endl;
        std::cout << "\tLatency:\n";
        std::cout << "\t\tmin: " << (double)count.min / 1000000 << " ms" << std::endl;
        std::cout << "\t\tmax: " << (double)count.max / 1000000 << " ms" << std::endl;
        std::cout << "\t\tavg: " << (double)count.avg / 1000000 << " ms" << std::endl;
        std::cout << "\t\tcount: " << count.count << std::endl;
    }
    saveProcessResult(count); // for multiple process
}
static void printResult(const Param& param, const Timer& timer, double counts = 0)
{
    if (!param.quiet) {
        double c = counts ? counts : param.processes * param.threads * param.loop;
        std::cout << "\nkernel execution(cmd queue length: " << param.bulk << "):\n";
        std::cout <<  "\tprocess(es): " << param.processes << std::endl;
        std::cout <<  "\tthread(s) per process: " << param.threads << std::endl;
        //std::cout <<  "\tnumber of executions per thread: " << param.loop << std::endl;
        std::cout <<  "\tthroughput: " << c / timer.elapsed() * 1000 << " ops/s (";
        std::cout << c << " executions in " << timer.elapsed() << " ms)" << std::endl;
    }
    saveProcessResult(timer, counts); // for multiple process
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
static void saveResult(const Param& param, const MaxT& maxT)
{
    std::ofstream handle(csv_file, std::ofstream::app);
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
    double tput = 0, avg = 0, ttput, tavg;
    boost::filesystem::directory_iterator dir(TMP), end;
    while (dir != end) {
        std::string fn = dir->path().filename().string();
        if (fn.rfind(EXT) != std::string::npos) {
            //std::cout << "fn: " << fn << " pid: " << getpid() << std::endl;
            if (fn.find(std::to_string(getpid()) + "_") != std::string::npos) {
                std::ifstream f(TMP + fn);
                if (f.is_open()) {
                    //std::cout << fn << std::endl;
                    std::string ret;
                    std::getline(f, ret);
                    min = std::min(min, std::atof(ret.c_str()));	
                    //std::cout << "min: " << std::atol(ret.c_str()) << std::endl;
                    std::getline(f, ret);
                    max = std::max(max, std::atof(ret.c_str()));	
                    //std::cout << "max: " << std::atol(ret.c_str()) << std::endl;
                    if (param.latency) {
                        std::getline(f, ret);
                        tavg = std::atof(ret.c_str());
                    }
                    //std::cout << "tput: " << std::atof(ret.c_str()) << std::endl;
                    std::getline(f, ret);
                    ttput = std::atof(ret.c_str());
                    if (param.latency)
                        avg = (avg * tput + tavg * ttput) / (ttput + tput);	
                    tput += ttput;
                    f.close();
                }
            }
            boost::filesystem::remove_all(dir->path());
        }
        dir++;
    }
    if (boost::filesystem::exists(TMP))
        boost::filesystem::remove_all(TMP);

    std::cout << "\nkernel execution(cmd queue length: " << param.bulk << "):\n";
    std::cout <<  "\tprocess(es): " << param.processes << std::endl;
    std::cout <<  "\tthread(s) per process: " << param.threads << std::endl;
    if (param.latency) {
        std::cout << "\tLatency:\n";
        std::cout << "\t\tmin: " << min/1000000 << " ms" << std::endl;
        std::cout << "\t\tmax: " << max/1000000 << " ms" << std::endl;
        std::cout << "\t\tavg: " << avg/1000000 << " ms" << std::endl;
        std::cout << "\t\tcount: " << tput << std::endl;
    } else {
        std::cout <<  "\tthroughput: " << tput *1000000000 / (max - min) << " ops/s (";
        std::cout << tput << " executions in " << (max - min)/1000000 << " ms)" << std::endl;
    }
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
    if (!strcasecmp(str, "lat"))
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
thr0(std::vector<xrt::run>&& cmds, int loop, const Timer& timer, Count& count)
{
    int issued = 0, completed = 0;
    uint32_t c = 0;
    long stamp[cmds.size()];
    for (auto& cmd : cmds) {
        if (count.lat)
            stamp[c++] = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        cmd.start();
        issued++;
    }

    c = 0;
    while (!loop || completed < loop) {
        auto state = cmds[c].wait(1000);
        switch (state) {
            case ERT_CMD_STATE_COMPLETED:
            case ERT_CMD_STATE_ERROR:
            case ERT_CMD_STATE_ABORT:
            {
                if (count.lat) {
                    auto end = std::chrono::high_resolution_clock::now().time_since_epoch().count();
                    auto delta = end - stamp[c];
                    stamp[c] = end;
                    count.min = std::min(count.min, delta);
                    count.max = std::max(count.max, delta);
                    count.avg = (delta + count.count * count.avg) / (count.count + 1);
                }
                count.count++;
                completed++;
                if (!loop || issued < loop) {
                    if (count.lat)
                        stamp[c] = std::chrono::high_resolution_clock::now().time_since_epoch().count();
                    cmds[c].start();
                    issued++;
                }
                break;
            }
            default:
                break;
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
    std::vector<std::vector<xrt::run>> cmds;
    std::vector<Count> count;
    for (c = 0; c < param.threads; c++) {
    	std::vector<xrt::run> cmd;
    	for (int i = 0; i < bulk; i++) {
        	auto run = xrt::run(hello);
        	run.set_arg(0, xrt::bo(device, 32, 0, hello.group_id(0)));
        	cmd.push_back(std::move(run));
    	}
       	cmds.push_back(std::move(cmd));
        count.push_back(Count{param.latency, LLONG_MAX, LLONG_MIN, 0, 0});
    }

    /*
     * For multiple threads case, the time of the thread setup and tear down is also
     * counted as the time running the kernel. This impact the accuracy.
     * Better way is, driver has statistics and can be reported by a tool like, custat
     */  
    Timer timer(param.time);
    if (param.threads == 1) {
        thr0(std::move(cmds[0]), param.time ? 0 : param.loop, timer, count[0]);
    } else {
        for (c = 0; c < param.threads; c++)
            thrs.emplace_back(&thr0, cmds[c], param.time ? 0 : param.loop, std::ref(timer), std::ref(count[c]));
        for (auto& t : thrs)
            t.join();
    }
    timer.stop();

    double total = 0;
    for (auto &c: count)
        total += c.count;
    if (!param.time && total != param.threads * param.loop)
        throw std::runtime_error("caculated wrong!! total = " + std::to_string(total));

    if (param.latency) {
        Count ret = {false, LLONG_MAX, LLONG_MIN, 0, 0};
        for (auto &t : count) {
            ret.min = std::min(t.min, ret.min);
            ret.max = std::max(t.max, ret.max);
            ret.avg = (ret.avg * ret.count + t.avg * t.count) / (ret.count + t.count);
            ret.count += t.count; 
        }
        printResult(param, ret);
    } else { 
        printResult(param, timer, total);
    
        MaxT nmaxT = {
            param.processes,
            param.threads,
            param.bulk,
            total * 1000 / timer.elapsed(),
        };
        if (nmaxT.tput > maxT.tput)
            maxT = std::move(nmaxT);
        }
    return 0;
}

int run(int argc, char** argv, char *envp[])
{
    std::string xclbin_fnm;
    bool quiet = false;
    bool lat = false;
    unsigned int device_index = 0;
    int loop = 30000;
    int threads = 1;
    int bulk = 32;
    int processes = 1;
    int c;
    int mode = 0;
    double time = 0;
    std::vector<char *> nargv;
    nargv.push_back(argv[0]);
    
    while ((c = getopt(argc, argv, "b:d:hk:m:n:p:qt:LT:")) != -1) {
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
            lat = (mode == 4);
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
  
    Param param = {device_index, processes, threads, bulk, loop, time, lat, quiet, xclbin_fnm};
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
        saveResult(param, maxT);
    } else if (mode == 2) { /*multiple process test*/
        std::cout << "\nMultiple process test...\n";
        if (param.processes == 1)
            std::cout << "Warning: -p to specify maximum processes!!!\n\n";
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
        if (param.threads == 1)
            std::cout << "Warning: -t to specify maximum threads!!!\n\n";
        auto t = make_p2(param.threads);
        if (t != param.threads) {
            std::cout << "Roundup threads to " << t << "(next power of 2)\n";
        }
        for (int i = 1; i <= t; i *= 2) {
            param.threads = i;
            run(param, maxT);
        }
    } else {
        if (processes > 1) {
            /*
             * when running multiple process test, we don't print number for each process/thread,
             * just print the whole instead.
             */  
            nargv.push_back((char *)"-q");
            if (lat)
                nargv.push_back((char *)"-L");
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
