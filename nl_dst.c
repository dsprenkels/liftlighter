#ifndef NL_DST_H
#define NL_DST_H

#include <inttypes.h>
#include <time.h>

int nl_dst(const time_t *timer, int32_t *_)
{
    struct tm tm;
    int8_t mday, n;

    gmtime_r(timer, &tm); // UTC time

    if (tm.tm_mon < MARCH || tm.tm_mon > OCTOBER) {
        return 0;
    }
    if (tm.tm_mon > MARCH && tm.tm_mon < OCTOBER) {
        return ONE_HOUR;
    }

    // either March or October. Both have 31 days. Get the last Sunday.
    mday = tm.tm_mday;  // current day of month
    mday -= tm.tm_wday; // date for *some* Sunday in this year
    n = (31 - mday);    // days till last day of month
    n /= 7;             // full weeks left in this month
    mday += n * 7;      // date of last sunday this month

    if (tm.tm_mon == MARCH) {
        if (tm.tm_mday < mday) return 0;
        if (tm.tm_mday > mday) return ONE_HOUR;
        if (tm.tm_hour < 1) return 0;
        return ONE_HOUR;
    } else /* October */ {
        if (tm.tm_mday < mday) return ONE_HOUR;
        if (tm.tm_mday > mday) return 0;
        if (tm.tm_hour < 1) return ONE_HOUR;
        return 0;
    }
}

#endif /* NL_DST_H */
