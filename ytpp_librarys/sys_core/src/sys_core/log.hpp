#pragma once

#include <iostream>

#ifndef YTPP_LOG_ENABLE
#define YTPP_LOG_ENABLE 1
#endif

#define YTPP_LOG_COLOR_RESET "\033[0m"
#define YTPP_LOG_COLOR_BLACK "\033[30m"
#define YTPP_LOG_COLOR_RED "\033[31m"
#define YTPP_LOG_COLOR_GREEN "\033[32m"
#define YTPP_LOG_COLOR_YELLOW "\033[33m"
#define YTPP_LOG_COLOR_BLUE "\033[34m"
#define YTPP_LOG_COLOR_MAGENTA "\033[35m"
#define YTPP_LOG_COLOR_CYAN "\033[36m"
#define YTPP_LOG_COLOR_WHITE "\033[37m"
#define YTPP_LOG_COLOR_BRIGHT_BLACK "\033[90m"
#define YTPP_LOG_COLOR_BRIGHT_RED "\033[91m"
#define YTPP_LOG_COLOR_BRIGHT_GREEN "\033[92m"
#define YTPP_LOG_COLOR_BRIGHT_YELLOW "\033[93m"
#define YTPP_LOG_COLOR_BRIGHT_BLUE "\033[94m"
#define YTPP_LOG_COLOR_BRIGHT_MAGENTA "\033[95m"
#define YTPP_LOG_COLOR_BRIGHT_CYAN "\033[96m"
#define YTPP_LOG_COLOR_BRIGHT_WHITE "\033[97m"

#if YTPP_LOG_ENABLE
#define YTPP_LOG_WRITE(color, value)                                                                                   \
    do {                                                                                                               \
        std::cout << color << value << YTPP_LOG_COLOR_RESET << std::endl;                                              \
    } while (false)
#else
#define YTPP_LOG_WRITE(color, value)                                                                                   \
    do {                                                                                                               \
    } while (false)
#endif

#define YTPP_LOG(value) YTPP_LOG_WRITE(YTPP_LOG_COLOR_RESET, value)
#define YTPP_LOG_NORMAL(value) YTPP_LOG_WRITE(YTPP_LOG_COLOR_RESET, value)
#define YTPP_LOG_BLACK(value) YTPP_LOG_WRITE(YTPP_LOG_COLOR_BLACK, value)
#define YTPP_LOG_RED(value) YTPP_LOG_WRITE(YTPP_LOG_COLOR_RED, value)
#define YTPP_LOG_GREEN(value) YTPP_LOG_WRITE(YTPP_LOG_COLOR_GREEN, value)
#define YTPP_LOG_YELLOW(value) YTPP_LOG_WRITE(YTPP_LOG_COLOR_YELLOW, value)
#define YTPP_LOG_BLUE(value) YTPP_LOG_WRITE(YTPP_LOG_COLOR_BLUE, value)
#define YTPP_LOG_MAGENTA(value) YTPP_LOG_WRITE(YTPP_LOG_COLOR_MAGENTA, value)
#define YTPP_LOG_CYAN(value) YTPP_LOG_WRITE(YTPP_LOG_COLOR_CYAN, value)
#define YTPP_LOG_WHITE(value) YTPP_LOG_WRITE(YTPP_LOG_COLOR_WHITE, value)
#define YTPP_LOG_GRAY(value) YTPP_LOG_WRITE(YTPP_LOG_COLOR_BRIGHT_BLACK, value)
#define YTPP_LOG_BRIGHT_RED(value) YTPP_LOG_WRITE(YTPP_LOG_COLOR_BRIGHT_RED, value)
#define YTPP_LOG_BRIGHT_GREEN(value) YTPP_LOG_WRITE(YTPP_LOG_COLOR_BRIGHT_GREEN, value)
#define YTPP_LOG_BRIGHT_YELLOW(value) YTPP_LOG_WRITE(YTPP_LOG_COLOR_BRIGHT_YELLOW, value)
#define YTPP_LOG_BRIGHT_BLUE(value) YTPP_LOG_WRITE(YTPP_LOG_COLOR_BRIGHT_BLUE, value)
#define YTPP_LOG_BRIGHT_MAGENTA(value) YTPP_LOG_WRITE(YTPP_LOG_COLOR_BRIGHT_MAGENTA, value)
#define YTPP_LOG_BRIGHT_CYAN(value) YTPP_LOG_WRITE(YTPP_LOG_COLOR_BRIGHT_CYAN, value)
#define YTPP_LOG_BRIGHT_WHITE(value) YTPP_LOG_WRITE(YTPP_LOG_COLOR_BRIGHT_WHITE, value)

#define YTPP_LOG_INFO(value) YTPP_LOG_CYAN("[INFO] " << value)
#define YTPP_LOG_SUCCESS(value) YTPP_LOG_GREEN("[SUCCESS] " << value)
#define YTPP_LOG_WARNING(value) YTPP_LOG_YELLOW("[WARNING] " << value)
#define YTPP_LOG_ERROR(value) YTPP_LOG_RED("[ERROR] " << value)
#define YTPP_LOG_DEBUG(value) YTPP_LOG_GRAY("[DEBUG] " << value)
#define YTPP_LOG_TRACE(value) YTPP_LOG_GRAY("[TRACE] " << value)
#define YTPP_LOG_FATAL(value) YTPP_LOG_BRIGHT_RED("[FATAL] " << value)
#define YTPP_LOG_NOTICE(value) YTPP_LOG_BLUE("[NOTICE] " << value)
