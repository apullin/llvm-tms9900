// Minimal time.h for TMS9900 freestanding libc++
#ifndef _TMS9900_TIME_H
#define _TMS9900_TIME_H

typedef __SIZE_TYPE__ size_t;
typedef long time_t;
typedef long clock_t;

#define CLOCKS_PER_SEC 1000000L
#define NULL ((void *)0)

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

struct timespec {
    time_t tv_sec;
    long   tv_nsec;
};

#ifdef __cplusplus
extern "C" {
#endif

clock_t clock(void);
double difftime(time_t __time1, time_t __time0);
time_t mktime(struct tm *__timeptr);
time_t time(time_t *__timer);
char *asctime(const struct tm *__timeptr);
char *ctime(const time_t *__timer);
struct tm *gmtime(const time_t *__timer);
struct tm *localtime(const time_t *__timer);
size_t strftime(char *__s, size_t __maxsize, const char *__format, const struct tm *__timeptr);
int timespec_get(struct timespec *__ts, int __base);

#ifdef __cplusplus
}
#endif

#endif // _TMS9900_TIME_H
