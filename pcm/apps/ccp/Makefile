CC ?= gcc # C compiler (use C compiler, not C++)
CXX ?= g++ # C++ compiler (used for linking when building for C++ projects)
DEBUG = n
LOGLEVEL = info

CFLAGS = -fPIC -Wall -Wextra -g # C flags
CFLAGS += -std=gnu99 -Wno-declaration-after-statement -fgnu89-inline
CXXFLAGS = -std=c++23 -Wall -Wextra -g -O3

ifeq ($(DEBUG), y)
	CFLAGS += -O0 -g
else
	CFLAGS += -O3 -g
endif

# logging level
ifeq ($(LOGLEVEL), trace)
	CFLAGS += -D__LOG_TRACE__
else ifeq ($(LOGLEVEL), debug)
	CFLAGS += -D__LOG_DEBUG__
else ifeq ($(LOGLEVEL), info)
	CFLAGS += -D__LOG_INFO__
else ifeq ($(LOGLEVEL), warn)
	CFLAGS += -D__LOG_WARN__
else ifeq ($(LOGLEVEL), err)
	CFLAGS += -D__LOG_ERROR__
endif

# profiling support
ifeq ($(PROFILE_DP_PROGRAM), y)
	CFLAGS += -DPROFILE_DP_PROGRAM
endif
ifeq ($(PROFILE_DP_IPC), y)
	CFLAGS += -DPROFILE_DP_IPC
endif

ifeq ($(PROFILE_SUBMIT), y)
	CXXFLAGS += -DPROFILE_SUBMIT
else ifeq ($(PROFILE_FULL_CYCLE), y)
	CXXFLAGS += -DPROFILE_FULL_CYCLE
else ifeq ($(PROFILE_ASYNC_INVOKE), y)
	CXXFLAGS += -DPROFILE_ASYNC_INVOKE
else ifeq ($(PROFILE_FETCH), y)
	CXXFLAGS += -DPROFILE_FETCH
endif

# Timing backend selection
ifeq ($(TIMING_BACKEND), PERF)
	CXXFLAGS += -DPERF_INSTR_TIMER
else ifeq ($(TIMING_BACKEND), CHRONO)
	CXXFLAGS += -DCHRONO_TIMER
endif


RM = rm -f  # rm command

LIB_NAME = ccp
TARGET_LIB = lib${LIB_NAME}.so # target lib
STATIC_TARGET = lib${LIB_NAME}.a

TEST_TARGET = unittest
TESTBENCH = testbench
TESTBENCH_PORTUS = testbench_with_portus
SRCS = ccp.c machine.c serialize.c ccp_priv.c # source files
OBJS = $(SRCS:.c=.o)

TEST_SRCS = unittest.c
TEST_OBJS = $(TEST_SRCS:.c=.o)

.PHONY: all
all: ${TARGET_LIB} ${STATIC_TARGET} test testbench

$(TARGET_LIB): $(OBJS)
	$(CXX) -shared ${LDFLAGS} -o $@ $^

$(STATIC_TARGET): $(OBJS)
	ar rcs $(STATIC_TARGET) $(OBJS)

$(SRCS:.c=.d):%.d:%.c
	$(CC) $(CFLAGS) -MM $< >$@

-include $(SRCS:.c=.d)

$(TEST_TARGET): ${STATIC_TARGET} ${TEST_OBJS}
	$(CXX) ${LDFLAGS} ${TEST_OBJS} ${STATIC_TARGET} -o ${TEST_TARGET}

test: $(TEST_TARGET)
	./$(TEST_TARGET)

# Build standalone testbench (no Portus)
$(TESTBENCH): testbench.cpp $(STATIC_TARGET)
	$(CXX) $(CXXFLAGS) -o $@ testbench.cpp -L. -l$(LIB_NAME) -I.

# Build testbench with Portus IPC support + PCM VM adapter
$(TESTBENCH_PORTUS): testbench_with_portus.cpp $(STATIC_TARGET)
	$(CXX) $(CXXFLAGS) -o $@ testbench_with_portus.cpp \
		-I. -I../pcm-sdk/pcm/include \
		-L. -l$(LIB_NAME) -lpthread \
		-L../pcm-sdk/pcm/build/lib -Wl,-rpath,../pcm-sdk/pcm/build/lib

.PHONY: testbench
testbench: $(TESTBENCH) $(TESTBENCH_PORTUS)

.PHONY: clean
clean:
	-${RM} ${TARGET_LIB} ${OBJS} ${TEST_OBJS} $(SRCS:.c=.d) ${TEST_TARGET} ${TESTBENCH} ${TESTBENCH_PORTUS} ${STATIC_TARGET}
	-${RM} -r *.dSYM
