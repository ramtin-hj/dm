#ifndef LOG_H
#define LOG_H

#define LOG(x)                  do { printf("%s\n", x); } while(0)
#define LOG_INFO(x)             do { printf("info: %s\n", x); } while(0)
#define LOG_WARN(x)             do { fprintf(stderr, "warning: %s\n", x); } while (0)
#define LOG_ERROR(x)            do { fprintf(stderr, "error: %s\n", x); } while (0)
#define LOG_FATAL(x)            do { fprintf(stderr, "fatal: %s\n", x); } while(0)
#define LOG_VA(fmt, ...)        do { printf(fmt "\n", __VA_ARGS__); } while(0)
#define LOG_INFO_VA(fmt, ...)   do { printf("info: " fmt "\n", __VA_ARGS__); } while(0)
#define LOG_WARN_VA(fmt, ...)   do { fprintf(stderr, "warning: " fmt "\n", __VA_ARGS__); } while(0)
#define LOG_ERROR_VA(fmt, ...)  do { fprintf(stderr, "error: " fmt "\n", __VA_ARGS__); } while(0)
#define LOG_FATAL_VA(fmt, ...)  do { fprintf(stderr, "fatal: " fmt "\n", __VA_ARGS__); } while(0)

#endif
