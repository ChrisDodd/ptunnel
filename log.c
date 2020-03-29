#include "log.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

const char *log_timestamp()
{
    static struct tm	last_full;
    static char		buffer[32];
    struct tm		now_tm;
    struct timeval	now;

    gettimeofday(&now, 0);
    localtime_r(&now.tv_sec, &now_tm);
    if (now_tm.tm_hour != last_full.tm_hour ||
	now_tm.tm_mday != last_full.tm_mday ||
	now_tm.tm_mon != last_full.tm_mon) {
	strftime(buffer, sizeof(buffer), "[%F %T.", &now_tm);
	last_full = now_tm;
    } else {
	strftime(buffer, sizeof(buffer), "[%M:%S.", &now_tm);
    }
    sprintf(buffer + strlen(buffer), "%06d] ", (int)now.tv_usec);
    return buffer;
}
