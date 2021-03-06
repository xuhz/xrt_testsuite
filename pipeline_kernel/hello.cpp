/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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
#include <stdexcept>
#include <string>

#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"
#include "experimental/xrt_bo.h"

/**
 * Runs an OpenCL kernel which writes "Hello World\n" into the buffer passed
 */

#define ARRAY_SIZE 20
////////////////////////////////////////////////////////////////////////////////

#define LENGTH (20)

////////////////////////////////////////////////////////////////////////////////

static const char gold[] = "Hello World\n";

static void usage()
{
    std::cout << "usage: %s [options] -k <bitstream>\n\n";
    std::cout << "  -k <bitstream>\n";
    std::cout << "  -d <index>\n";
    std::cout << "  -v\n";
    std::cout << "  -h\n\n";
    std::cout << "* Bitstream is required\n";
}

static void
run(const xrt::device& device, const xrt::uuid& uuid, bool verbose)
{
  auto m2s = xrt::kernel(device, uuid.get(), "m2s:m2s_1");
  auto s2m = xrt::kernel(device, uuid.get(), "s2m:s2m_1");
  auto boin = xrt::bo(device, 1024, 0, m2s.group_id(0));
  std::cout << "In bo created" << std::endl;
  auto bo_data_in = boin.map<char*>();
  std::fill(bo_data_in, bo_data_in + 1024, 'a');
  boin.sync(XCL_BO_SYNC_BO_TO_DEVICE, 1024,0);
  std::cout << "In bo synced" << std::endl;
  auto boout = xrt::bo(device, 1024, 0, s2m.group_id(1));
  auto bo_data_out = boout.map<char*>();
  std::cout << "Out bo created" << std::endl;

  auto runm2s = xrt::run(m2s);
  runm2s.set_arg(0,boin);
  runm2s.set_arg(2,1024/4);
  auto runs2m = xrt::run(s2m);
  runs2m.set_arg(1,boout);
  runs2m.set_arg(2,1024/4);
  std::cout << "Kernel m2s start command issued" << std::endl;
  runm2s.start();
  std::cout << "Kernel s2m start command issued" << std::endl;
  runs2m.start();
  std::cout << "Now wait until the m2s kernel finish" << std::endl;
  runm2s.wait();
  std::cout << "Now wait until the s2m kernel finish" << std::endl;
  runs2m.wait();

  //Get the output;
  std::cout << "Get the output data from the device" << std::endl;
  boout.sync(XCL_BO_SYNC_BO_FROM_DEVICE, 1024, 0);

  std::cout << "RESULT: " << std::endl;
  for (unsigned i = 0; i < 32; ++i)
    std::cout << bo_data_out[i];
  std::cout << std::endl;
}


int run(int argc, char** argv)
{
  if (argc < 3) {
    usage();
    return 1;
  }

  std::string xclbin_fnm;
  bool verbose = false;
  unsigned int device_index = 0;

  std::vector<std::string> args(argv+1,argv+argc);
  std::string cur;
  for (auto& arg : args) {
    if (arg == "-h") {
      usage();
      return 1;
    }
    else if (arg == "-v") {
      verbose = true;
      continue;
    }

    if (arg[0] == '-') {
      cur = arg;
      continue;
    }

    if (cur == "-k")
      xclbin_fnm = arg;
    else if (cur == "-d")
      device_index = std::stoi(arg);
    else
      throw std::runtime_error("Unknown option value " + cur + " " + arg);
  }

  if (xclbin_fnm.empty())
    throw std::runtime_error("FAILED_TEST\nNo xclbin specified");

  if (device_index >= xclProbe())
    throw std::runtime_error("Cannot find device index (" + std::to_string(device_index) + ") specified");

  auto device = xrt::device(device_index);
  auto uuid = device.load_xclbin(xclbin_fnm);

  run(device, uuid, verbose);
  return 0;
}

int main(int argc, char** argv)
{
  try {
    auto ret = run(argc, argv);
    std::cout << "PASSED TEST\n";
    return ret;
  }
  catch (std::exception const& e) {
    std::cout << "Exception: " << e.what() << "\n";
    std::cout << "FAILED TEST\n";
    return 1;
  }

  std::cout << "PASSED TEST\n";
  return 0;
}
