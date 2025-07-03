ifeq ($(origin CC),default)
CC     := clang
CXX    := clang++
endif

CFLAGS  := -fPIC -Iinclude -Iapp -Ialgorithms -Wall -Wextra -O3
CXXFLAGS := $(CFLAGS) -std=c++17
LDFLAGS := -shared

SRCDIR           := src
HTSIM_SUBDIR     := $(SRCDIR)/htsim
INCDIR           := include
ALGODIR          := algorithms
LIBDIR           := lib
BINDIR           := bin
APPSDIR          := apps

# Core PCM sources (C only)
PCM_C_SRCS       := $(wildcard $(SRCDIR)/*.c) $(wildcard $(HTSIM_SUBDIR)/*.c)
PCM_CPP_SRCS     :=

# Optionally Linux perf-based profiling
ENABLE_PROFILING ?= 0
ifeq ($(ENABLE_PROFILING),1)
PROFILING_PCM_LIB_CFLAGS := -DENABLE_PROFILING
endif

# Optionally include HTSIM plugin sources and application
BUILD_HTSIM_FLOW_PLUGIN ?= 0
ifeq ($(BUILD_HTSIM_FLOW_PLUGIN),1)

# Root of external HTSIM simulation code
HTSIM_ROOT_DIR ?= ../../HTSIM/sim/

PCM_CPP_SRCS := $(wildcard $(HTSIM_SUBDIR)/*.cpp)
HTSIM_PCM_LIB_CFLAGS := -DBUILD_HTSIM_FLOW_PLUGIN -I$(HTSIM_ROOT_DIR) -I$(HTSIM_ROOT_DIR)/datacenter
HTSIM_PCM_LIB_LDLIBS := -L$(HTSIM_ROOT_DIR) -lhtsim -lstdc++

HTSIM_APP_SRCS := $(APPSDIR)/htsim
HTSIM_APP_OBJS := $(HTSIM_APP_SRCS)/htsim_app.o
HTSIM_APP_TARGET := $(BINDIR)/htsim_flow_app
HTSIM_APP_CFLAGS := -I$(SRCDIR)/htsim/
HTSIM_APP_PREREQ_OBJS := \
	$(HTSIM_ROOT_DIR)/libhtsim.a \
	$(HTSIM_ROOT_DIR)/datacenter/firstfit.o \
	$(HTSIM_ROOT_DIR)/datacenter/vl2_topology.o \
	$(HTSIM_ROOT_DIR)/datacenter/fat_tree_topology.o \
	$(HTSIM_ROOT_DIR)/datacenter/fat_tree_interdc_topology.o \
	$(HTSIM_ROOT_DIR)/datacenter/fat_tree_switch.o \
	$(HTSIM_ROOT_DIR)/datacenter/fat_tree_interdc_switch.o \
	$(HTSIM_ROOT_DIR)/datacenter/bcube_topology.o \
	$(HTSIM_ROOT_DIR)/datacenter/connection_matrix.o \
	$(HTSIM_ROOT_DIR)/datacenter/oversubscribed_fat_tree_topology.o \
	$(HTSIM_ROOT_DIR)/datacenter/shortflows.o \
	$(HTSIM_ROOT_DIR)/datacenter/multihomed_fat_tree_topology.o \
	$(HTSIM_ROOT_DIR)/datacenter/star_topology.o \
	$(HTSIM_ROOT_DIR)/datacenter/logsim-interface.o
endif

# Gather all object files
PCM_C_OBJS       := $(PCM_C_SRCS:%.c=%.o)
PCM_CPP_OBJS     := $(PCM_CPP_SRCS:%.cpp=%.o)
PCM_LIB_OBJS     := $(PCM_C_OBJS) $(PCM_CPP_OBJS)
PCM_LIB_TARGET   := $(LIBDIR)/libpcm.so

ALGO_PLUGIN_NAMES := reno_accumulated reno_standard dctcp swift dcqcn smartt
ALGO_PLUGIN_LIBS  := $(addprefix $(LIBDIR)/lib,$(addsuffix .so,$(ALGO_PLUGIN_NAMES)))
ALGO_PLUGIN_LIBS_LL  := $(addprefix $(LIBDIR)/lib,$(addsuffix .ll,$(ALGO_PLUGIN_NAMES)))
ALGO_PLUGIN_PCMC_NAMES := tcp swift dcqcn smartt
ALGO_PLUGIN_PCMC_LIBS  := $(addprefix $(LIBDIR)/lib,$(addsuffix _pcmc.so,$(ALGO_PLUGIN_PCMC_NAMES)))
ALGO_PLUGIN_PCMC_LDLIBS  := $(addprefix -l,$(addsuffix _pcmc,$(ALGO_PLUGIN_PCMC_NAMES)))

TGEN_APP_SRCS     := $(APPSDIR)/traffic_gen
TGEN_APP_OBJS     := $(TGEN_APP_SRCS)/main.o $(TGEN_APP_SRCS)/traffic_gen.o
TGEN_APP_TARGET   := $(BINDIR)/traffic_gen_app

.PHONY: all clean distclean
# Include empty HTSIM_APP_TARGET if disabled
all: $(PCM_LIB_TARGET) $(ALGO_PLUGIN_LIBS) $(ALGO_PLUGIN_PCMC_LIBS) $(ALGO_PLUGIN_LIBS_LL) $(TGEN_APP_TARGET) $(HTSIM_APP_TARGET)

# -----------------------------------------------
# PCM core
# -----------------------------------------------

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(HTSIM_PCM_LIB_CFLAGS) $(PROFILING_PCM_LIB_CFLAGS) -c $< -o $@

# htsim plugin C++ sources
ifeq ($(BUILD_HTSIM_FLOW_PLUGIN),1)
$(HTSIM_SUBDIR)/%.o: $(HTSIM_SUBDIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(HTSIM_PCM_LIB_CFLAGS) $(PROFILING_PCM_LIB_CFLAGS) -c $< -o $@
endif

# Link into libpcm.so
$(PCM_LIB_TARGET): $(PCM_LIB_OBJS)
	@mkdir -p $(LIBDIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(HTSIM_PCM_LIB_LDLIBS)

# -----------------------------------------------
# Algorithm plugins
# -----------------------------------------------

# host-side PCMC init code
$(LIBDIR)/lib%_pcmc.so: $(ALGODIR)/%_pcmc.c $(PCM_LIB_TARGET)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(LDFLAGS) \
	    -o $@ $< -L$(LIBDIR) -lpcm

# DCTCP is special as it needs BUILD_ALGO_DCTCP
$(LIBDIR)/libdctcp.so: $(ALGODIR)/dctcp.c $(PCM_LIB_TARGET)
	@mkdir -p $(LIBDIR)
	$(CC) $(CFLAGS) -DHANDLER_BUILD -DBUILD_ALGO_DCTCP $(LDFLAGS) \
	    -o $@ $< -L$(LIBDIR) -lpcm -lm

# other algos
$(LIBDIR)/lib%.so: $(ALGODIR)/%.c $(PCM_LIB_TARGET)
	@mkdir -p $(LIBDIR)
	$(CC) $(CFLAGS) -DHANDLER_BUILD $(LDFLAGS) -o $@ $< -L$(LIBDIR) -lpcm -lm


#-----------------------------------------------
# LLVM IR of the handlers
#-----------------------------------------------

# DCTCP is special as it needs BUILD_ALGO_DCTCP  (why? looks ugly)
$(LIBDIR)/libdctcp.ll: $(ALGODIR)/dctcp.c $(PCM_LIB_TARGET)
	@mkdir -p $(LIBDIR)
	clang -S -emit-llvm $(CFLAGS) -DHANDLER_BUILD -DBUILD_ALGO_DCTCP -o $@ $<

# other algos
$(LIBDIR)/lib%.ll: $(ALGODIR)/%.c $(PCM_LIB_TARGET)
	@mkdir -p $(LIBDIR)
	clang -S -emit-llvm $(CFLAGS) -DHANDLER_BUILD -o $@ $< 



# -----------------------------------------------
# Traffic generator app
# -----------------------------------------------
$(TGEN_APP_SRCS)/%.o: $(TGEN_APP_SRCS)/%.c
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TGEN_APP_TARGET): $(TGEN_APP_OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $^ -L$(LIBDIR) $(ALGO_PLUGIN_PCMC_LDLIBS) -lpcm -o $@

# -----------------------------------------------
# HTSIM application
# -----------------------------------------------

ifeq ($(BUILD_HTSIM_FLOW_PLUGIN),1)

$(HTSIM_APP_SRCS)/%.o: $(HTSIM_APP_SRCS)/%.cpp
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) $(HTSIM_PCM_LIB_CFLAGS) $(HTSIM_APP_CFLAGS) -c $< -o $@

$(HTSIM_APP_TARGET): $(HTSIM_APP_PREREQ_OBJS) $(HTSIM_APP_OBJS)
	@mkdir -p $(BINDIR)
	$(CXX) -o $@ $^ $(HTSIM_PCM_LIB_LDLIBS) -L$(LIBDIR) $(ALGO_PLUGIN_PCMC_LDLIBS) -lpcm

endif

clean:
	@echo "Cleaning build artifacts..."
	@find $(SRCDIR) -name '*.o' -delete
	@find $(HTSIM_SUBDIR) -name '*.o' -delete
	@find $(TGEN_APP_SRCS) -name '*.o' -delete
	@rm -f $(PCM_LIB_TARGET)
	@rm -f $(ALGO_PLUGIN_LIBS)
	@rm -f $(ALGO_PLUGIN_PCMC_LIBS)
	@rm -f $(TGEN_APP_TARGET)
	@if [ -n "$(HTSIM_APP_TARGET)" ]; then rm -f $(HTSIM_APP_TARGET); fi
	@if [ -n "$(HTSIM_APP_TARGET)" ]; then rm -f $(HTSIM_APP_OBJS); fi
distclean: clean
	rm -rf $(LIBDIR) $(BINDIR)
