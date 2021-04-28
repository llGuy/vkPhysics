#if 0

#pragma once

#include <stdio.h>
#include <utility>

extern FILE *g_logfile;

void init_log_file();

#define LOG_TO_CONSOLE

template <typename ...T>
inline void log_impl(
    const char *prompt,
    const char *function,
    const char *format,
    T &&...ts) {
    // TODO: Add colors
#if defined (LOG_TO_CONSOLE)
    printf("%s:%s:  ", prompt, function);
    printf(format, ts...);
#else
    fprintf(g_logfile, "%s:%s:  ", prompt, function);
    fprintf(g_logfile, format, ts...);
    // In case of a crash
    fflush(g_logfile);
#endif
}

#define LOG_ERRORV(str, ...) log_impl("ERROR", __FUNCTION__, str, __VA_ARGS__)
#define LOG_WARNINGV(str, ...) log_impl("WARNING", __FUNCTION__, str, __VA_ARGS__)
#define LOG_INFOV(str, ...) log_impl("INFO", __FUNCTION__, str, __VA_ARGS__)
#define LOG_ERROR(str) log_impl("ERROR", __FUNCTION__, str)
#define LOG_WARNING(str) log_impl("WARNING", __FUNCTION__, str)
#define LOG_INFO(str) log_impl("INFO", __FUNCTION__, str)


#endif
#pragma once

#include <stdio.h>

#define LOG_ERRORV(str, ...)                    \
    printf("ERROR:%s:  ", __FUNCTION__);  \
    printf(str, __VA_ARGS__)

#define LOG_WARNINGV(str, ...)                          \
    printf("WARNING:%s:  ", __FUNCTION__);        \
    printf(str, __VA_ARGS__)

#define LOG_INFOV(str, ...)                     \
    printf("INFO:%s:  ", __FUNCTION__);   \
    printf(str, __VA_ARGS__)

#define LOG_ERROR(str)                          \
    printf("ERROR:%s:  ", __FUNCTION__);  \
    printf(str)

#define LOG_WARNING(str)                                \
    printf("WARNING:%s:  ", __FUNCTION__);        \
    printf(str)

#define LOG_INFO(str)                           \
    printf("INFO:%s:  ", __FUNCTION__);   \
    printf(str)
