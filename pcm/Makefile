ifeq ($(origin CC),default)
CC     := clang
CXX    := clang++
endif

CFLAGS  := -fPIC -Iinclude -Iapp -Ialgorithms -Wall -Wextra -O3
LDFLAGS := -shared

SRCDIR := src
INCDIR := include
ALGODIR := algorithms
LIBDIR := lib
BINDIR := bin
APPSDIR := apps

PCM_LIB_SRCS := $(wildcard $(SRCDIR)/*.c)

# htsim related variables to enable PCM htsim plugin and application
BUILD_HTSIM_FLOW_PLUGIN ?= 0
ifeq ($(BUILD_HTSIM_FLOW_PLUGIN),1)
  HTSIM_ROOT_DIR ?= ../../HTSIM/sim/
  PCM_LIB_CFLAGS := -DBUILD_HTSIM_FLOW_PLUGIN -I$(HTSIM_ROOT_DIR) -I$(HTSIM_ROOT_DIR)/datacenter
  PCM_LIB_LDLIBS := -L$(HTSIM_ROOT_DIR) -lhtsim -lstdc++
  HTSIM_APP_SRCS := $(APPSDIR)/htsim/
  # objects from htsim build
  HTSIM_APP_PREREQ_OBJS := $(HTSIM_ROOT_DIR)/libhtsim.a \
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
  HTSIM_APP_OBJS := $(HTSIM_APP_SRCS)/htsim_app.o
  HTSIM_APP_TARGET := $(BINDIR)/htsim_app
  HTSIM_APP_CFLAGS := -std=c++17
else
  PCM_LIB_CFLAGS :=
  PCM_LIB_LDLIBS :=
endif

PCM_LIB_OBJS   := $(PCM_LIB_SRCS:%.c=%.o)
PCM_LIB_TARGET := $(LIBDIR)/libpcm.so

ALGO_PLUGIN_NAMES := reno_accumulated reno_standard dctcp swift dcqcn smartt
ALGO_PLUGIN_LIBS  := $(addprefix $(LIBDIR)/lib,$(addsuffix .so,$(ALGO_PLUGIN_NAMES)))

TGEN_APP_SRCS     := $(APPSDIR)/traffic_gen/
TGEN_APP_OBJS     := $(TGEN_APP_SRCS)/main.o $(TGEN_APP_SRCS)/traffic_gen.o
TGEN_APP_TARGET   := $(BINDIR)/traffic_gen_app

.PHONY: all clean distclean
all: $(PCM_LIB_TARGET) $(ALGO_PLUGIN_LIBS) $(TGEN_APP_TARGET) $(HTSIM_APP_TARGET)

# build libpcm.so
$(PCM_LIB_TARGET): $(PCM_LIB_OBJS)
	@mkdir -p $(LIBDIR)
	$(CC) $(LDFLAGS) $(PCM_LIB_LDLIBS) -o $@ $^

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(PCM_LIB_CFLAGS) -c $< -o $@

# build algorithm plugins: lib<algoname>.so
$(LIBDIR)/libdctcp.so: $(ALGODIR)/dctcp.c $(PCM_LIB_TARGET)
	@mkdir -p $(LIBDIR)
<<<<<<< HEAD
	$(CC) $(CFLAGS) -DHANDLER_BUILD -DBUILD_ALGO_DCTCP $(LDFLAGS) -o $@ $(ALGODIR)/dctcp.c -L$(LIBDIR) -lpcm -lm
=======
	$(CC) $(CFLAGS) -DHANDLER_BUILD -DBUILD_ALGO_DCTCP $(LDFLAGS) \
	    -o $@ $< -L$(LIBDIR) -lpcm
>>>>>>> 4114757 (add htsim flow plugin)

$(LIBDIR)/lib%.so: $(ALGODIR)/%.c $(PCM_LIB_TARGET)
	@mkdir -p $(LIBDIR)
<<<<<<< HEAD
	$(CC) $(CFLAGS) -DHANDLER_BUILD $(LDFLAGS) -o $@ $(ALGODIR)/$*.c -L$(LIBDIR) -lpcm -lm
=======
	$(CC) $(CFLAGS) -DHANDLER_BUILD $(LDFLAGS) \
	    -o $@ $< -L$(LIBDIR) -lpcm
>>>>>>> 4114757 (add htsim flow plugin)

# build the traffic_gen application
$(TGEN_APP_SRCS)/%.o: $(TGEN_APP_SRCS)/%.c
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TGEN_APP_TARGET): $(TGEN_APP_OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $^ -L$(LIBDIR) -lpcm -o $@

# build the htsim application if requested
ifeq ($(BUILD_HTSIM_FLOW_PLUGIN),1)

$(HTSIM_APP_SRCS)/%.o: $(HTSIM_APP_SRCS)/%.cpp
	@mkdir -p $(BINDIR)
	$(CXX) $(CFLAGS) $(PCM_LIB_CFLAGS) $(HTSIM_APP_CFLAGS) -c $< -o $@

$(HTSIM_APP_TARGET): $(HTSIM_APP_PREREQ_OBJS) $(HTSIM_APP_OBJS)
	$(CXX) -o $@ $^ $(PCM_LIB_LDLIBS)

endif

clean:
	rm -f $(PCM_LIB_OBJS) $(ALGO_PLUGIN_LIBS) $(TGEN_APP_OBJS) $(TGEN_APP_TARGET)

distclean: clean
	rm -rf $(LIBDIR) $(BINDIR)
