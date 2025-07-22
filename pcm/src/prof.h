#ifndef _PROF_H_
#define _PROF_H_

#ifdef ENABLE_PROFILING

#ifdef __linux__
#include <asm/unistd.h>
#include <linux/perf_event.h>

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                            int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

#define PERF_PROF_REGION_SCOPE_INIT()                                          \
    int fd_cycles, fd_instr;                                                   \
    uint64_t id_cycles, id_instr;

#define PERF_PROF_REGION_START()                                               \
    {                                                                          \
        struct perf_event_attr pe_cycles = {0};                                \
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
        fd_cycles = perf_event_open(&pe_cycles, 0, -1, -1, 0);                 \
        if (fd_cycles == -1)                                                   \
            PCM_LOG_FATAL("prof perf_event_open (cycles)");                    \
                                                                               \
        /* Open instructions in group */                                       \
        fd_instr = perf_event_open(&pe_instr, 0, -1, fd_cycles, 0);            \
        if (fd_instr == -1)                                                    \
            PCM_LOG_FATAL("prof perf_event_open (instructions)");              \
                                                                               \
        /* Get IDs to map values later */                                      \
        if (ioctl(fd_cycles, PERF_EVENT_IOC_ID, &id_cycles))                   \
            PCM_LOG_FATAL("prof ioctl PERF_EVENT_IOC_ID failed");              \
        if (ioctl(fd_instr, PERF_EVENT_IOC_ID, &id_instr))                     \
            PCM_LOG_FATAL("prof ioctl PERF_EVENT_IOC_ID failed");              \
                                                                               \
        if (ioctl(fd_cycles, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP))       \
            PCM_LOG_FATAL("prof ioctl PERF_EVENT_IOC_RESET failed");           \
                                                                               \
        if (ioctl(fd_cycles, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP))      \
            PCM_LOG_FATAL("prof ioctl PERF_EVENT_IOC_ENABLE failed");          \
    }

#define PERF_PROF_REGION_END(report, region_name)                              \
    {                                                                          \
        if (ioctl(fd_cycles, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP))     \
            PCM_LOG_FATAL("prof ioctl PERF_EVENT_IOC_DISABLE failed");         \
                                                                               \
        struct {                                                               \
            uint64_t nr;                                                       \
            struct {                                                           \
                uint64_t value;                                                \
                uint64_t id;                                                   \
            } values[2];                                                       \
        } data;                                                                \
                                                                               \
        if (read(fd_cycles, &data, sizeof(data)) != sizeof(data))              \
            PCM_LOG_FATAL("prof read failed");                                 \
                                                                               \
        uint64_t cycles, instructions;                                         \
        for (uint64_t j = 0; j < data.nr; ++j) {                               \
            if (data.values[j].id == id_cycles) {                              \
                cycles = data.values[j].value;                                 \
            } else if (data.values[j].id == id_instr) {                        \
                instructions = data.values[j].value;                           \
            }                                                                  \
        }                                                                      \
                                                                               \
        if (close(fd_cycles))                                                  \
            PCM_LOG_FATAL("prof close(fd_cycles) failed");                     \
        if (close(fd_instr))                                                   \
            PCM_LOG_FATAL("prof close(fd_instr) failed");                      \
                                                                               \
        if (report)                                                            \
            PCM_PCM_LOG_PRINT("%s %lu cycles %lu instructions\n", region_name, \
                              cycles, instructions);                           \
    }
#endif

#else

#define PERF_PROF_REGION_SCOPE_INIT()                                          \
    {                                                                          \
    }
#define PERF_PROF_REGION_START()                                               \
    {                                                                          \
    }
#define PERF_PROF_REGION_END(report, region_name)                              \
    {                                                                          \
    }

#endif

#endif /* _PROF_H_ */