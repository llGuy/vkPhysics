#pragma once

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
