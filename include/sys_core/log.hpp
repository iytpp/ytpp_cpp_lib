#pragma once

#include <iostream>

// ============================================================
// 日志总开关
//
// 1 = 开启日志
// 0 = 关闭日志
// ============================================================

#ifndef LOG_ENABLE
#define LOG_ENABLE 1
#endif


// ============================================================
// ANSI / Windows Terminal 颜色
// Windows 10 / 11 控制台支持 ANSI VT Escape Sequence
// ============================================================

#define LOG_COLOR_RESET           "\033[0m"

// 普通颜色
#define LOG_COLOR_BLACK           "\033[30m"
#define LOG_COLOR_RED             "\033[31m"
#define LOG_COLOR_GREEN           "\033[32m"
#define LOG_COLOR_YELLOW          "\033[33m"
#define LOG_COLOR_BLUE            "\033[34m"
#define LOG_COLOR_MAGENTA         "\033[35m"
#define LOG_COLOR_CYAN            "\033[36m"
#define LOG_COLOR_WHITE           "\033[37m"

// 高亮颜色
#define LOG_COLOR_BRIGHT_BLACK    "\033[90m"
#define LOG_COLOR_BRIGHT_RED      "\033[91m"
#define LOG_COLOR_BRIGHT_GREEN    "\033[92m"
#define LOG_COLOR_BRIGHT_YELLOW   "\033[93m"
#define LOG_COLOR_BRIGHT_BLUE     "\033[94m"
#define LOG_COLOR_BRIGHT_MAGENTA  "\033[95m"
#define LOG_COLOR_BRIGHT_CYAN     "\033[96m"
#define LOG_COLOR_BRIGHT_WHITE    "\033[97m"


// ============================================================
// 日志宏
// ============================================================

#if LOG_ENABLE

#define LOG(x) \
    do { \
        std::cout << x << std::endl; \
    } while (0)

#define LOG_NORMAL(x) \
    do { \
        std::cout << LOG_COLOR_RESET \
                  << x \
                  << LOG_COLOR_RESET \
                  << std::endl; \
    } while (0)

#define LOG_BLACK(x) \
    do { \
        std::cout << LOG_COLOR_BLACK \
                  << x \
                  << LOG_COLOR_RESET \
                  << std::endl; \
    } while (0)

#define LOG_RED(x) \
    do { \
        std::cout << LOG_COLOR_RED \
                  << x \
                  << LOG_COLOR_RESET \
                  << std::endl; \
    } while (0)

#define LOG_GREEN(x) \
    do { \
        std::cout << LOG_COLOR_GREEN \
                  << x \
                  << LOG_COLOR_RESET \
                  << std::endl; \
    } while (0)

#define LOG_YELLOW(x) \
    do { \
        std::cout << LOG_COLOR_YELLOW \
                  << x \
                  << LOG_COLOR_RESET \
                  << std::endl; \
    } while (0)

#define LOG_BLUE(x) \
    do { \
        std::cout << LOG_COLOR_BLUE \
                  << x \
                  << LOG_COLOR_RESET \
                  << std::endl; \
    } while (0)

#define LOG_MAGENTA(x) \
    do { \
        std::cout << LOG_COLOR_MAGENTA \
                  << x \
                  << LOG_COLOR_RESET \
                  << std::endl; \
    } while (0)

#define LOG_CYAN(x) \
    do { \
        std::cout << LOG_COLOR_CYAN \
                  << x \
                  << LOG_COLOR_RESET \
                  << std::endl; \
    } while (0)

#define LOG_WHITE(x) \
    do { \
        std::cout << LOG_COLOR_WHITE \
                  << x \
                  << LOG_COLOR_RESET \
                  << std::endl; \
    } while (0)

#define LOG_GRAY(x) \
    do { \
        std::cout << LOG_COLOR_BRIGHT_BLACK \
                  << x \
                  << LOG_COLOR_RESET \
                  << std::endl; \
    } while (0)

#define LOG_BRIGHT_RED(x) \
    do { \
        std::cout << LOG_COLOR_BRIGHT_RED \
                  << x \
                  << LOG_COLOR_RESET \
                  << std::endl; \
    } while (0)

#define LOG_BRIGHT_GREEN(x) \
    do { \
        std::cout << LOG_COLOR_BRIGHT_GREEN \
                  << x \
                  << LOG_COLOR_RESET \
                  << std::endl; \
    } while (0)

#define LOG_BRIGHT_YELLOW(x) \
    do { \
        std::cout << LOG_COLOR_BRIGHT_YELLOW \
                  << x \
                  << LOG_COLOR_RESET \
                  << std::endl; \
    } while (0)

#define LOG_BRIGHT_BLUE(x) \
    do { \
        std::cout << LOG_COLOR_BRIGHT_BLUE \
                  << x \
                  << LOG_COLOR_RESET \
                  << std::endl; \
    } while (0)

#define LOG_BRIGHT_MAGENTA(x) \
    do { \
        std::cout << LOG_COLOR_BRIGHT_MAGENTA \
                  << x \
                  << LOG_COLOR_RESET \
                  << std::endl; \
    } while (0)

#define LOG_BRIGHT_CYAN(x) \
    do { \
        std::cout << LOG_COLOR_BRIGHT_CYAN \
                  << x \
                  << LOG_COLOR_RESET \
                  << std::endl; \
    } while (0)

#define LOG_BRIGHT_WHITE(x) \
    do { \
        std::cout << LOG_COLOR_BRIGHT_WHITE \
                  << x \
                  << LOG_COLOR_RESET \
                  << std::endl; \
    } while (0)

#else

#define LOG(x)                 do {} while (0)
#define LOG_NORMAL(x)          do {} while (0)
#define LOG_BLACK(x)           do {} while (0)
#define LOG_RED(x)             do {} while (0)
#define LOG_GREEN(x)           do {} while (0)
#define LOG_YELLOW(x)          do {} while (0)
#define LOG_BLUE(x)            do {} while (0)
#define LOG_MAGENTA(x)         do {} while (0)
#define LOG_CYAN(x)            do {} while (0)
#define LOG_WHITE(x)           do {} while (0)
#define LOG_GRAY(x)            do {} while (0)
#define LOG_BRIGHT_RED(x)      do {} while (0)
#define LOG_BRIGHT_GREEN(x)    do {} while (0)
#define LOG_BRIGHT_YELLOW(x)   do {} while (0)
#define LOG_BRIGHT_BLUE(x)     do {} while (0)
#define LOG_BRIGHT_MAGENTA(x)  do {} while (0)
#define LOG_BRIGHT_CYAN(x)     do {} while (0)
#define LOG_BRIGHT_WHITE(x)    do {} while (0)


// ============================================================
// 语义日志宏
// ============================================================

#define LOG_INFO(x) \
    LOG_CYAN("[INFO] " << x)

#define LOG_SUCCESS(x) \
    LOG_GREEN("[SUCCESS] " << x)

#define LOG_WARNING(x) \
    LOG_YELLOW("[WARNING] " << x)

#define LOG_ERROR(x) \
    LOG_RED("[ERROR] " << x)

#define LOG_DEBUG(x) \
    LOG_GRAY("[DEBUG] " << x)

#define LOG_TRACE(x) \
    LOG_BRIGHT_BLACK("[TRACE] " << x)

#define LOG_FATAL(x) \
    LOG_BRIGHT_RED("[FATAL] " << x)

#define LOG_NOTICE(x) \
    LOG_BLUE("[NOTICE] " << x)

#endif