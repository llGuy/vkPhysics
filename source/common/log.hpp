#pragma once

#include <stdio.h>

extern FILE *g_logfile;

void init_log_file();

#define LOG_ERRORV(str, ...)                    \
    fprintf(g_logfile, "ERROR:%s:  ", __FUNCTION__);  \
    fprintf(g_logfile, str, __VA_ARGS__); \
    fflush(g_logfile)

#define LOG_WARNINGV(str, ...)                          \
    fprintf(g_logfile, "WARNING:%s:  ", __FUNCTION__);        \
    fprintf(g_logfile, str, __VA_ARGS__);\
    fflush(g_logfile)

#define LOG_INFOV(str, ...)                     \
    fprintf(g_logfile, "INFO:%s:  ", __FUNCTION__);   \
    fprintf(g_logfile, str, __VA_ARGS__);\
    fflush(g_logfile)

#define LOG_ERROR(str)                          \
    fprintf(g_logfile, "ERROR:%s:  ", __FUNCTION__);  \
    fprintf(g_logfile, str);\
    fflush(g_logfile)

#define LOG_WARNING(str)                                \
    fprintf(g_logfile, "WARNING:%s:  ", __FUNCTION__);        \
    fprintf(g_logfile, str);\
    fflush(g_logfile)

#define LOG_INFO(str)                           \
    fprintf(g_logfile, "INFO:%s:  ", __FUNCTION__);   \
    fprintf(g_logfile, str);\
    fflush(g_logfile)
