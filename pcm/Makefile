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

ALGO_PLUGIN_NAMES := newreno dctcp swift dcqcn smartt
ALGO_PLUGIN_LIBS  := $(addprefix $(LIBDIR)/lib,$(addsuffix .so,$(ALGO_PLUGIN_NAMES)))
ALGO_PLUGIN_PCMC_LIBS  := $(addprefix $(LIBDIR)/lib,$(addsuffix _pcmc.so,$(ALGO_PLUGIN_NAMES)))
ALGO_PLUGIN_PCMC_LDLIBS  := $(addprefix -l,$(addsuffix _pcmc,$(ALGO_PLUGIN_NAMES)))

# compile HAC-related optimizaitons if enabled
ifeq ($(BUILD_HAC_PASS),1)
ALGO_PLUGIN_LIBS_LL  := $(addprefix $(LIBDIR)/lib,$(addsuffix .ll,$(ALGO_PLUGIN_NAMES)))
endif

TGEN_APP_SRCS     := $(APPSDIR)/traffic_gen
TGEN_APP_OBJS     := $(TGEN_APP_SRCS)/main.o $(TGEN_APP_SRCS)/traffic_gen.o
TGEN_APP_TARGET   := $(BINDIR)/traffic_gen_app

HAC_PASS := HAC/build/ReplaceCallWithLoad.so

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

# Special cases for dctcp and newreno with additional prerequisites
$(LIBDIR)/libdctcp.so: $(ALGODIR)/dctcp.c $(ALGODIR)/tcp_utils.c $(PCM_LIB_TARGET)
	@mkdir -p $(LIBDIR)
	$(CC) $(CFLAGS) -DHANDLER_BUILD -DBUILD_ALGO_DCTCP $(LDFLAGS) \
		-o $@ $(filter %.c,$^) -L$(LIBDIR) -lpcm -lm

$(LIBDIR)/libnewreno.so: $(ALGODIR)/newreno.c $(ALGODIR)/tcp_utils.c $(PCM_LIB_TARGET)
	@mkdir -p $(LIBDIR)
	$(CC) $(CFLAGS) -DHANDLER_BUILD -DBUILD_ALGO_NEWRENO $(LDFLAGS) \
		-o $@ $(filter %.c,$^) -L$(LIBDIR) -lpcm -lm

# General rule for other algorithms
$(LIBDIR)/lib%.so: $(ALGODIR)/%.c $(PCM_LIB_TARGET)
	@mkdir -p $(LIBDIR)
	$(CC) $(CFLAGS) \
		-DHANDLER_BUILD \
		-DBUILD_ALGO_$(shell echo $* | tr '[:lower:]' '[:upper:]') $(LDFLAGS) \
		-o $@ \
		$(filter %.c,$^) \
		-L$(LIBDIR) -lpcm -lm

# host-side PCMC init code
$(LIBDIR)/lib%_pcmc.so: $(ALGODIR)/%_pcmc.c $(PCM_LIB_TARGET)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(LDFLAGS) \
	    -o $@ $< -L$(LIBDIR) -lpcm

#-----------------------------------------------
# HAC-optimized handlers (dirty, make an actual HAC script and clean up)
#-----------------------------------------------

$(LIBDIR)/libdctcp.ll: $(ALGODIR)/dctcp.c $(ALGODIR)/tcp_utils.c $(PCM_LIB_TARGET) $(HAC_PASS)
	@mkdir -p $(LIBDIR)
	clang -S -emit-llvm $(CFLAGS) -DHANDLER_BUILD -DBUILD_ALGO_DCTCP -o $@ $(filter %.c,$^)
	opt -load-pass-plugin $(HAC_PASS) -passes=replace-call-with-load -S $@ -o $@_opt.ll
	opt -O3 $@_opt.ll -o $@
	@rm $@_opt.ll
	llc -filetype=obj -relocation-model=pic $@ -o $@.o
	$(CC) $(CFLAGS) -DHANDLER_BUILD $(LDFLAGS) -o $@.so $@.o -L$(LIBDIR) -lpcm -lm
	@rm $@.o

$(LIBDIR)/libnewreno.ll: $(ALGODIR)/newreno.c $(ALGODIR)/tcp_utils.c $(PCM_LIB_TARGET) $(HAC_PASS)
	@mkdir -p $(LIBDIR)
	clang -S -emit-llvm $(CFLAGS) -DHANDLER_BUILD -DBUILD_ALGO_NEWRENO -o $@ $(filter %.c,$^)
	opt -load-pass-plugin $(HAC_PASS) -passes=replace-call-with-load -S $@ -o $@_opt.ll
	opt -O3 $@_opt.ll -o $@
	@rm $@_opt.ll
	llc -filetype=obj -relocation-model=pic $@ -o $@.o
	$(CC) $(CFLAGS) -DHANDLER_BUILD $(LDFLAGS) -o $@.so $@.o -L$(LIBDIR) -lpcm -lm
	@rm $@.o

# General rule for other algorithms
$(LIBDIR)/lib%.ll: $(ALGODIR)/%.c $(PCM_LIB_TARGET) $(HAC_PASS)
	@mkdir -p $(LIBDIR)
	clang -S -emit-llvm $(CFLAGS) \
		-DHANDLER_BUILD \
		-DBUILD_ALGO_$(shell echo $* | tr '[:lower:]' '[:upper:]') \
		-o $@ $(filter %.c,$^)
	opt -load-pass-plugin $(HAC_PASS) -passes=replace-call-with-load -S $@ -o $@_opt.ll
	opt -O3 $@_opt.ll -o $@
	@rm $@_opt.ll
	llc -filetype=obj -relocation-model=pic $@ -o $@.o
	$(CC) $(CFLAGS) -DHANDLER_BUILD $(LDFLAGS) -o $@.so $@.o -L$(LIBDIR) -lpcm -lm
	@rm $@.o

# -----------------------------------------------
# Build the HAC LLVM pass
# -----------------------------------------------
$(HAC_PASS):
	cd HAC; rm -rf build; mkdir build; cd build; cmake ..; make; 

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
	@rm -rf HAC/build
	@rm -f $(LIBDIR)/*.ll $(LIBDIR)/*.ll.so
	@rm -rf $(LIBDIR)/*.dSYM
distclean: clean
	rm -rf $(LIBDIR) $(BINDIR)
