/**
 * @file
 * Tests if <tt>setitimer</tt> works with the minimum specifiable resolution.
 * SUSv3 says that:
 * <blockquote>
 * Implementations may place limitations on the granularity of timer values.
 * For each interval timer, if the requested timer value requires a finer
 * granularity than the implementation supports, the actual timer value shall
 * be rounded up to the next supported value.
 * </blockquote>
 * If an implementation does not comply and instead rounds down, it will
 * disable the timer:
 * <blockquote>
 * Setting it_value to 0 shall disable a timer, regardless of the value of
 * it_interval.
 * </blockquote>
 * See <a
 * href="http://www.opengroup.org/onlinepubs/007904975/functions/setitimer.html">the
 * specification</a>.
 * @legal
 * Copyright &copy; 2004 &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

int error_wrap(int retval, const char *funcname) {
    if (retval < 0) {
        fprintf(stderr, "%s returned %d (errno==%d) (%s)\n",
                funcname, retval, errno, strerror(errno));
        abort();
    }
    return retval;
}

volatile sig_atomic_t sigalrm_received;

void sigalrm_handler(int signum) { sigalrm_received = 1; }

int main(void) {
    struct itimerval it;
    struct timespec ts;
    int retval;

    /* Handle SIGALRM */
    signal(SIGALRM, &sigalrm_handler);

    /* Generate a SIGALRM as soon as possible. */
    it.it_value.tv_sec = 0;
    it.it_value.tv_usec = 1;
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = 0;
    error_wrap(setitimer(ITIMER_REAL, &it, NULL), "setitimer");

    /*
     * Wait for the alarm to be generated and delivered, giving plenty of
     * leeway.
     */
    ts.tv_sec = 1;
    ts.tv_nsec = 0;
    while ((retval = nanosleep(&ts, &ts)) == -1 && errno == EINTR) ;
    error_wrap(retval, "nanosleep");

    if (sigalrm_received) {
        printf("SIGALRM received; good.\n");
        return 0;
    } else {
        printf("SIGALRM was lost.\n");
        return 1;
    }
}
