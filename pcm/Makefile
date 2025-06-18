# Compiler and flags
ifeq ($(origin CC),default)
CC = clang
endif
CFLAGS = -fPIC -Iinclude -Iapp -Wall -Wextra -O3
LDFLAGS = -shared

SRCDIR = src
INCDIR = include
APPDIR = app
LIBDIR = lib
BINDIR = bin

# Library sources and objects
PCM_LIB_SRCS := $(wildcard $(SRCDIR)/*.c)
PCM_LIB_OBJS := $(PCM_LIB_SRCS:.c=.o)
PCM_LIB_TARGET := $(LIBDIR)/libpcm.so

# Plugin names and targets
ALGO_PLUGIN_NAMES := reno_accumulated reno_standard dctcp swift dcqcn smartt
ALGO_PLUGIN_LIBS  := $(addprefix $(LIBDIR)/lib,$(addsuffix .so,$(ALGO_PLUGIN_NAMES)))

# Application target
APP_OBJS := $(BINDIR)/main.o
APP_TARGET := $(BINDIR)/app_main

.PHONY: all clean distclean

all: $(PCM_LIB_TARGET) $(ALGO_PLUGIN_LIBS) $(APP_TARGET)

# Build main library
$(PCM_LIB_TARGET): $(PCM_LIB_OBJS)
	@mkdir -p $(LIBDIR)
	$(CC) $(LDFLAGS) -o $@ $^

# Compile library .o files (output PCM_LIB_OBJS)
$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build dctcp plugin (requires BUILD_ALGO_DCTCP)
$(LIBDIR)/libdctcp.so: $(APPDIR)/dctcp.c $(PCM_LIB_TARGET)
	@mkdir -p $(LIBDIR)
	$(CC) $(CFLAGS) -DHANDLER_BUILD -DBUILD_ALGO_DCTCP $(LDFLAGS) -o $@ $(APPDIR)/dctcp.c -L$(LIBDIR) -lpcm

# Build other algorithm plugins
$(LIBDIR)/lib%.so: $(APPDIR)/%.c $(PCM_LIB_TARGET)
	@mkdir -p $(LIBDIR)
	$(CC) $(CFLAGS) -DHANDLER_BUILD $(LDFLAGS) -o $@ $(APPDIR)/$*.c -L$(LIBDIR) -lpcm

# Compile main application object
$(BINDIR)/main.o: $(APPDIR)/main.c $(APPDIR)/tcp.h
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link application
$(APP_TARGET): $(APP_OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $^ -L$(LIBDIR) -lpcm -o $@

# Remove build artifacts
clean:
	rm -f $(PCM_LIB_OBJS) $(ALGO_PLUGIN_LIBS) $(APP_OBJS) $(APP_TARGET)

# Remove everything (including directories)
distclean: clean
	rm -rf $(LIBDIR) $(BINDIR)
