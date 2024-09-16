#ifndef LOGGER_H
#define LOGGER_H

typedef enum {
    LDEBUG,
    LINFO,
    LWARN,
    LERR,
    LMAX
} loglevel;

//#define LOG_PRINT(level, fmt, ...) logprint(level, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOG_PRINT(level, fmt, ...) logprint(level, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
//#define LOG_DEBUG(fmt, ...) LOG_PRINT(LDEBUG, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(...) LOG_PRINT(LDEBUG, __VA_ARGS__, "")
#define LOG_INFO(...)  LOG_PRINT(LINFO , __VA_ARGS__, "")
#define LOG_WARN(...)  LOG_PRINT(LWARN, __VA_ARGS__, "")
#define LOG_ERROR(...) LOG_PRINT(LERR, __VA_ARGS__, "")

int loginit(const char* filename);
void loglevel_on(loglevel level);
void loglevel_off(loglevel level);
void logprint(loglevel level, const char* file, int line, const char* func, const char* fmt, ...);

#endif /* LOGGER_H */
