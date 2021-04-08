#+----------------------------------------------------------------------------
#| Vitis Makefile
#| This make file is generated when the test is run
#| Generated: 2021-04-06 00:36:42.503056
#+----------------------------------------------------------------------------
# default CPU architecture:
CPU_ARCH = x86

# target and command definitions
CXX = g++
XCXX = v++
VIVADO = vivado
EMCONFIGUTIL = emconfigutil
RUNDIR=$(PWD)
RM = rm -rf
CPY = cp -prf
TARGET = hw

PLATFORM = ${PLATFORM_PATH}/xilinx_u30_gen3x4_1_202020_1/xilinx_u30_gen3x4_1_202020_1.xpfm
DEVICE = xilinx_u30_gen3x4_1_202020_1

# Common compilers settings
XCXX_COMMON_OPTS = -s --config global.ini  -t $(TARGET) -I .

# Host settings
TARGET_FLAGS = -DTARGET_FLOW_HW=1 -DTARGET_FLOW_HW_EMU=0 -DTARGET_FLOW_SW_EMU=0

# User defined host flags:
HOST_CFLAGS = -g -std=c++1y -Wall -Wno-narrowing -I ${XILINX_XRT}/include -L ${XILINX_XRT}/lib -lstdc++ -lxrt_coreutil -lxrt_core -lrt -luuid -lboost_system -lboost_filesystem -pthread  

# Default template host flags:
HOST_CFLAGS += $(TARGET_FLAGS) -DTARGET_DEVICE=\"$(DEVICE)\"
ifeq ($(CPU_ARCH),power9)
	HOST_CFLAGS += -std=gnu++14
else
	HOST_CFLAGS += -std=c++1y
endif
HOST_CFLAGS += -D_GLIBCXX_USE_CXX11_ABI=0

# Host files
HOST_PREAMBLE = 
HOST_EXE = host.exe
HOST_SRC = multi-card.cpp plugin_dec.cpp xrt_utils.cpp
HOST_ARGS = -d 0,1 -T 10 
HOST_EXEC_SCRIPT = /proj/xtools/dsv/projects/sprite/xrt_qor_host_exec.sh
# Set up the emconfigutil run
EMCONFIG_FILE = emconfig.json

NUM_DEVICES = 1

# Primary build targets
.PHONY: all build host run clean cleanall help

all: clean build run

build:
	@echo "INFO: Host code only flow. Running a dummy build target."

run: host
# Host Exec Script
ifeq ($(TARGET),$(filter $(TARGET),sw_emu hw_emu))
	$(EMCONFIGUTIL) --platform $(PLATFORM) --nd $(NUM_DEVICES)
	@echo "Running Emulation:"
	. xtc_env_vars.sh; ${HOST_EXEC_SCRIPT}
else ifeq ($(TARGET),$(filter $(TARGET),hw))
	@echo "Running HW Test:"
	. xtc_env_vars.sh; ${HOST_EXEC_SCRIPT}
endif

# Host rules
host: $(HOST_SRC)
	$(CXX) $+ -o $(HOST_EXE) $(HOST_CFLAGS)
	@echo "INFO: Compiled Host Executable: $(HOST_EXE)"

clean:
	$(RM) $(EMCONFIG_FILE) $(HOST_EXE)

help:
	 @echo 'Makefile usage:'
	 @echo '  make all TARGET=<sw_emu/hw_emu/hw> PLATFORM=<FPGA platform XPFM>'
	 @echo '      Clean the environment, build host, build xclbin and run the application.'
	 @echo ''
	 @echo '  make run TARGET=<sw_emu/hw_emu/hw> PLATFORM=<FPGA platform XPFM>'
	 @echo '      Build host, build xclbin and run the application.'
	 @echo ''
	 @echo '  make build TARGET=<sw_emu/hw_emu/hw> PLATFORM=<FPGA platform XPFM>'
	 @echo '      Build xclbin.'
	 @echo ''
	 @echo '  make host'
	 @echo '      Build host executable.'
	 @echo ''
	 @echo '  make clean '
	 @echo '      Remove the host executable and emconfig file.'
	 @echo ''
	 @echo '  make cleanall'
	 @echo '      Remove all, keep only the sources.'

