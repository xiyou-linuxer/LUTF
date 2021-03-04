#ifndef SET_TICKER_H
#define SET_TICKER_H

#include <unistd.h>
#include <sys/time.h>

int set_ticker(int n_msecs);

int set_ticker(int n_msecs)
{
    struct itimerval new_timeset;
    long n_sec, n_usecs;

    n_sec = n_msecs / 1000;
    n_usecs = (n_msecs % 1000) * 1000L;

    //剩下的时间，tv.sec = 秒，tv.usec = 微秒
    new_timeset.it_interval.tv_sec = n_sec;
    new_timeset.it_interval.tv_usec = n_usecs;

    //间隔时间
    new_timeset.it_value.tv_sec = n_sec;
    new_timeset.it_value.tv_usec = n_usecs;

    return setitimer(ITIMER_REAL, &new_timeset, NULL);
}

#endif
