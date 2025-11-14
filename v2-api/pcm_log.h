#ifndef _PCM_LOG_H_
#define _PCM_LOG_H_

#include <stdlib.h>
#include <lwlog.h>

#define PCM_LOG_DBG(FORMAT, ...)                                               \
    {                                                                          \
        lwlog_debug(FORMAT, ##__VA_ARGS__);                                    \
    }

#define PCM_LOG_INFO(FORMAT, ...)                                              \
    {                                                                          \
        lwlog_info(FORMAT, ##__VA_ARGS__);                                     \
    }

#define PCM_LOG_CRIT(FORMAT, ...)                                              \
    {                                                                          \
        lwlog_crit(FORMAT, ##__VA_ARGS__);                                     \
    }

#define PCM_LOG_PRINT(FORMAT, ...)                                             \
    {                                                                          \
        lwlog_info(FORMAT, ##__VA_ARGS__);                                     \
    }

#define PCM_LOG_FATAL(FORMAT, ...)                                             \
    {                                                                          \
        lwlog_crit(FORMAT, ##__VA_ARGS__);                                     \
        exit(EXIT_FAILURE);                                                    \
    }

#define PCM_EXIT_ON_ERR(call)                                                  \
    do {                                                                       \
        int _err = (call);                                                     \
        if (_err != PCM_SUCCESS) {                                             \
            PCM_LOG_FATAL("'%s' failed with code %d at %s:%d", #call, _err,    \
                          __FILE__, __LINE__);                                 \
        }                                                                      \
    } while (0)

#endif /* _PCMC_LOG_H_ */