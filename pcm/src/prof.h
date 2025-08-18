#ifndef _PROF_H_
#define _PROF_H_

#ifdef ENABLE_PROFILING

#include "../include/pcm_log.h"

#ifdef __linux__
#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                            int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

struct pcm_perf_context {
    int fd_cycles;
    int fd_instr;
    uint64_t id_cycles;
    uint64_t id_instr;
    const char *scope_prefix;
};

#define PCM_PERF_PROF_REGION_SCOPE_INIT(perf_obj, scope_prefix_str)            \
    struct pcm_perf_context perf_obj = {                                       \
        .fd_cycles = 0,                                                        \
        .fd_instr = 0,                                                         \
        .id_cycles = 0,                                                        \
        .id_instr = 0,                                                         \
        .scope_prefix = scope_prefix_str,                                      \
    };

#define PCM_PERF_PROF_REGION_START(perf_obj)                                   \
    {                                                                          \
        struct perf_event_attr pe_cycles;                                      \
        memset(&pe_cycles, 0, sizeof(pe_cycles));                              \
        pe_cycles.type = PERF_TYPE_HARDWARE;                                   \
        pe_cycles.size = sizeof(struct perf_event_attr);                       \
        pe_cycles.config = PERF_COUNT_HW_CPU_CYCLES;                           \
        pe_cycles.disabled = 1;                                                \
        pe_cycles.exclude_kernel = 1;                                          \
        pe_cycles.exclude_hv = 1;                                              \
        pe_cycles.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;            \
                                                                               \
        struct perf_event_attr pe_instr = pe_cycles;                           \
        pe_instr.config = PERF_COUNT_HW_INSTRUCTIONS;                          \
                                                                               \
        /* Open group leader (cycles) */                                       \
        perf_obj.fd_cycles = perf_event_open(&pe_cycles, 0, -1, -1, 0);        \
        if (perf_obj.fd_cycles == -1)                                          \
            PCM_LOG_FATAL("pcm-perf: %s prof perf_event_open (cycles)",        \
                          perf_obj.scope_prefix);                              \
                                                                               \
        /* Open instructions in group */                                       \
        perf_obj.fd_instr =                                                    \
            perf_event_open(&pe_instr, 0, -1, perf_obj.fd_cycles, 0);          \
        if (perf_obj.fd_instr == -1)                                           \
            PCM_LOG_FATAL("pcm-perf: %s prof perf_event_open (instructions)",  \
                          perf_obj.scope_prefix);                              \
                                                                               \
        /* Get IDs to map values later */                                      \
        if (ioctl(perf_obj.fd_cycles, PERF_EVENT_IOC_ID, &perf_obj.id_cycles)) \
            PCM_LOG_FATAL("pcm-perf: %s prof ioctl PERF_EVENT_IOC_ID failed",  \
                          perf_obj.scope_prefix);                              \
        if (ioctl(perf_obj.fd_instr, PERF_EVENT_IOC_ID, &perf_obj.id_instr))   \
            PCM_LOG_FATAL("pcm-perf: %s prof ioctl PERF_EVENT_IOC_ID failed",  \
                          perf_obj.scope_prefix);                              \
                                                                               \
        if (ioctl(perf_obj.fd_cycles, PERF_EVENT_IOC_RESET,                    \
                  PERF_IOC_FLAG_GROUP))                                        \
            PCM_LOG_FATAL(                                                     \
                "pcm-perf: %s prof ioctl PERF_EVENT_IOC_RESET failed",         \
                perf_obj.scope_prefix);                                        \
                                                                               \
        if (ioctl(perf_obj.fd_cycles, PERF_EVENT_IOC_ENABLE,                   \
                  PERF_IOC_FLAG_GROUP))                                        \
            PCM_LOG_FATAL(                                                     \
                "pcm-perf: %s prof ioctl PERF_EVENT_IOC_ENABLE failed",        \
                perf_obj.scope_prefix);                                        \
    }

#define PCM_PERF_PROF_REGION_END(perf_obj, report)                             \
    {                                                                          \
        if (ioctl(perf_obj.fd_cycles, PERF_EVENT_IOC_DISABLE,                  \
                  PERF_IOC_FLAG_GROUP))                                        \
            PCM_LOG_FATAL(                                                     \
                "pcm-perf: %s prof ioctl PERF_EVENT_IOC_DISABLE failed",       \
                perf_obj.scope_prefix);                                        \
                                                                               \
        struct {                                                               \
            uint64_t nr;                                                       \
            struct {                                                           \
                uint64_t value;                                                \
                uint64_t id;                                                   \
            } values[2];                                                       \
        } data;                                                                \
                                                                               \
        if (read(perf_obj.fd_cycles, &data, sizeof(data)) != sizeof(data))     \
            PCM_LOG_FATAL("pcm-perf: %s prof read failed",                     \
                          perf_obj.scope_prefix);                              \
                                                                               \
        uint64_t cycles, instructions;                                         \
        for (uint64_t j = 0; j < data.nr; ++j) {                               \
            if (data.values[j].id == perf_obj.id_cycles) {                     \
                cycles = data.values[j].value;                                 \
            } else if (data.values[j].id == perf_obj.id_instr) {               \
                instructions = data.values[j].value;                           \
            }                                                                  \
        }                                                                      \
                                                                               \
        if (close(perf_obj.fd_cycles))                                         \
            PCM_LOG_FATAL("pcm-perf: %s prof close(fd_cycles) failed",         \
                          perf_obj.scope_prefix);                              \
        if (close(perf_obj.fd_instr))                                          \
            PCM_LOG_FATAL("pcm-perf: %s prof close(fd_instr) failed",          \
                          perf_obj.scope_prefix);                              \
                                                                               \
        if (report)                                                            \
            PCM_LOG_PRINT("pcm-perf: %s %lu cycles %lu instructions\n",        \
                          perf_obj.scope_prefix, cycles, instructions);        \
    }
#endif

#else

#define PCM_PERF_PROF_REGION_SCOPE_INIT(perf_obj, scope_prefix_str)            \
    {                                                                          \
    }
#define PCM_PERF_PROF_REGION_START(perf_obj)                                   \
    {                                                                          \
    }

#define PCM_PERF_PROF_REGION_END(perf_obj, report)                             \
    {                                                                          \
    }

#endif

#endif /* _PROF_H_ */