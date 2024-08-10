#ifndef LOGGER_H
#define LOGGER_H

typedef enum {
    LDEBUG,
    LINFO,
    LWARN,
    LERR,
    LMAX
} loglevel;

#define LOGPRINT(level, fmt, ...) logprint(level, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOGDEBUG(fmt, ...) LOGPRINT(LDEBUG, fmt, ##__VA_ARGS__)
#define LOGINFO(fmt, ...)  LOGPRINT(LINFO , fmt, ##__VA_ARGS__)
#define LOGWARN(fmt, ...)  LOGPRINT(LWARN, fmt, ##__VA_ARGS__)
#define LOGERROR(fmt, ...) LOGPRINT(LERR, fmt, ##__VA_ARGS__)

int loginit(const char* filename);
void loglevel_on(loglevel level);
void loglevel_off(loglevel level);
void logprint(loglevel level, const char* file, int line, const char* func, const char* fmt, ...);

#endif /* LOGGER_H */
