#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
//#include <execinfo.h>
#include "logger.h"


FILE* logfile;
mtx_t mtx;

#define LFILE logfile ? logfile : stderr

_Atomic int level_en[LMAX] = {0, 0, 0, 0};

#include <time.h>

#include<sys/time.h>

long long timeInMilliseconds(void) {
    struct timeval tv;

    gettimeofday(&tv,NULL);
    return (((long long)tv.tv_sec)*1000)+(tv.tv_usec/1000);
}

int loginit(const char* filename)
{
    //void* dummy = NULL;

	/* trigger libgcc load to memory); */
    //backtrace(&dummy, 1);

	mtx_init(&mtx, mtx_plain);
	mtx_lock(&mtx);
	if ((logfile = fopen(filename, "a")) != NULL) {
		mtx_unlock(&mtx);
		return EXIT_SUCCESS;
	} else {
		mtx_unlock(&mtx);
        	fprintf(stderr, "Error opening input file %s: %s! Falling back to stderr\n", filename, strerror(errno));
		return EXIT_FAILURE;
	}
}

void loglevel_on(loglevel level)
{
	level_en[level] = 1;
}

void loglevel_off(loglevel level)
{
	level_en[level] = 0;
}

void logprint(loglevel level, const char* file, int line, const char* func, const char* fmt, ...)
{
	char buf[sizeof "9999-12-31 23:59:59"];
	char* levels[LMAX] = {"[DEBUG]", "[INFO]", "[WARN]", "[ERROR]"};
	struct timeval tv;
	va_list args;

	if (level_en[level]) {
		mtx_lock(&mtx);
		gettimeofday(&tv, NULL);
		strftime(buf, sizeof(buf), "%F %T", localtime(&tv.tv_sec));
		fprintf(LFILE, "%s.%d %s ", buf, (int) (tv.tv_usec / 1000), levels[level]);
		fprintf(LFILE, "%s:%d (%s) ", file, line, func);
		va_start(args, fmt);
		vfprintf(LFILE, fmt, args);
		va_end(args);
		fprintf(LFILE, "\n");
		fflush(LFILE);
		// if ((level_en[LERR]) && (level == LERR)) {
		// 	void* symbols[100];
		// 	int n = backtrace(symbols, 100);
		// 	backtrace_symbols_fd(symbols, n, fileno(LFILE));
		// 	fflush(LFILE);
		// }
		mtx_unlock(&mtx);
	}
}