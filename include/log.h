#ifndef LOG_H
#define LOG_H

#include "ascii.h"

#ifdef NDEBUG
#define LOG(x)                  do { printf("%s\n", x); } while(0)
#define LOG_INFO(x)             do { printf(ASCII_BLUE "info: " ASCII_RESET "%s\n", x); } while(0)
#define LOG_WARN(x)             do { fprintf(stderr, ASCII_YELLOW "warning: " ASCII_RESET "%s\n", x); } while (0)
#define LOG_ERROR(x)            do { fprintf(stderr, ASCII_RED "error: " ASCII_RESET "%s\n", x); } while (0)
#define LOG_FATAL(x)            do { fprintf(stderr, ASCII_RED_BOLD "fatal: " ASCII_RESET "%s\n", x); } while(0)
#define LOG_VA(fmt, ...)        do { printf(fmt "\n", __VA_ARGS__); } while(0)
#define LOG_INFO_VA(fmt, ...)   do { printf(ASCII_BLUE "info: " ASCII_RESET fmt "\n", __VA_ARGS__); } while(0)
#define LOG_WARN_VA(fmt, ...)   do { fprintf(stderr, ASCII_YELLOW "warning: " ASCII_RESET fmt "\n", __VA_ARGS__); } while(0)
#define LOG_ERROR_VA(fmt, ...)  do { fprintf(stderr, ASCII_RED "error: " ASCII_RESET fmt "\n", __VA_ARGS__); } while(0)
#define LOG_FATAL_VA(fmt, ...)  do { fprintf(stderr, ASCII_RED_BOLD "fatal: " ASCII_RESET fmt "\n", __VA_ARGS__); } while(0)
#else
#define LOG(x)                  do { printf("[%d] %s\n", __LINE__, x); } while (0)
#define LOG_INFO(x)             do { printf(ASCII_BLUE "info: " ASCII_RESET "[%d] %s\n", __LINE__, x); } while (0)
#define LOG_WARN(x)             do { fprintf(stderr, ASCII_YELLOW "warning: " ASCII_RESET "[%d] %s\n", __LINE__, x); } while (0)
#define LOG_ERROR(x)            do { fprintf(stderr, ASCII_RED "error: " ASCII_RESET "[%d] %s\n", __LINE__, x); } while (0)
#define LOG_FATAL(x)            do { fprintf(stderr, ASCII_RED_BOLD "fatal: " ASCII_RESET "[%d] %s\n", __LINE__, x); } while (0)
#define LOG_VA(fmt, ...)        do { printf("[%d] " fmt "\n", __LINE__, __VA_ARGS__); } while (0)
#define LOG_INFO_VA(fmt, ...)   do { printf(ASCII_BLUE "info: " ASCII_RESET "[%d] " fmt "\n", __LINE__, __VA_ARGS__); } while (0)
#define LOG_WARN_VA(fmt, ...)   do { fprintf(stderr, ASCII_YELLOW "warning: " ASCII_RESET "[%d] " fmt "\n", __LINE__, __VA_ARGS__); } while (0)
#define LOG_ERROR_VA(fmt, ...)  do { fprintf(stderr, ASCII_RED "error: " ASCII_RESET "[%d] " fmt "\n", __LINE__, __VA_ARGS__); } while (0)
#define LOG_FATAL_VA(fmt, ...)  do { fprintf(stderr, ASCII_RED_BOLD "fatal: " ASCII_RESET "[%d] " fmt "\n", __LINE__, __VA_ARGS__); } while (0)
#endif

#endif
