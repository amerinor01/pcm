ifeq ($(origin CC),default)
CC     := clang
endif

CFLAGS  := -fPIC -Iinclude -Iapp -Ialgorithms -Wall -Wextra -O3
LDFLAGS := -shared

SRCDIR           := src
INCDIR           := include
APPDIR           := app
ALGODIR          := algorithms
LIBDIR           := lib
BINDIR           := bin

PCM_LIB_SRCS := $(wildcard $(SRCDIR)/*.c)

HTSIM_ROOT_DIR   ?= ../../HTSIM/sim/
BUILD_HTSIM_FLOW_PLUGIN ?= 0

ifeq ($(BUILD_HTSIM_FLOW_PLUGIN),1)
  PCM_LIB_CFLAGS := -DBUILD_HTSIM_FLOW_PLUGIN -I$(HTSIM_ROOT_DIR)
  PCM_LIB_LDLIBS := -L$(HTSIM_ROOT_DIR) -lhtsim -lstdc++
else
  PCM_LIB_CFLAGS :=
  PCM_LIB_LDLIBS :=
endif

PCM_LIB_OBJS   := $(PCM_LIB_SRCS:%.c=%.o)
PCM_LIB_TARGET := $(LIBDIR)/libpcm.so

ALGO_PLUGIN_NAMES := reno_accumulated reno_standard dctcp swift dcqcn smartt
ALGO_PLUGIN_LIBS  := $(addprefix $(LIBDIR)/lib,$(addsuffix .so,$(ALGO_PLUGIN_NAMES)))

APP_OBJS     := $(BINDIR)/main.o $(BINDIR)/app_traffic_gen.o
APP_TARGET   := $(BINDIR)/app_main

.PHONY: all clean distclean
all: $(PCM_LIB_TARGET) $(ALGO_PLUGIN_LIBS) $(APP_TARGET)

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

# build the app_main
$(BINDIR)/%.o: $(APPDIR)/%.c
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(APP_TARGET): $(APP_OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $^ -L$(LIBDIR) -lpcm -o $@

clean:
	rm -f $(PCM_LIB_OBJS) $(ALGO_PLUGIN_LIBS) $(APP_OBJS) $(APP_TARGET)

distclean: clean
	rm -rf $(LIBDIR) $(BINDIR)
