/** @file
 * Tests if signal delivery can cause us to lose track of received bytes.
 * The read documentation in SUSv3 includes:
 *
 *      "If a read() is interrupted by a signal after it has successfully read
 *      some data, it shall return the number of bytes read."
 *
 * In this case, it seems reasonable to think that the signal handler would be
 * called first. If it jumps (as we do) rather than returning to the kernel,
 * this byte count may be discarded.
 *
 * If I find a platform on which this test fails (loops forever), I'll have to
 * turn off SA_RESTART and ensure we don't ever jump from the actual system
 * call instruction, just immediately before it. I may do that anyway, for
 * safety.
 *
 * @legal
 * Copyright &copy 2004 &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#include <limits.h>
#include <unistd.h>
#include <setjmp.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>
#include <sigsafe.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define min(a,b) ((a)<(b)?(a):(b))

enum {
    READ = 0,
    WRITE
} PipeHalf;

#ifndef PIPE_BUF
/* OSF/1 doesn't */
#define PIPE_BUF _POSIX_PIPE_BUF
#endif

/* Write a lot of bytes */
#define BYTES_TO_TRANSFER (PIPE_BUF<<16)/*4294967295u*/

/* ...in groups of more than PIPE_BUF to maximize the chances of something
 * interesting happening. */
#define SINGLE_TRANSFER (16*PIPE_BUF)

#define USECS_BETWEEN_SIGNALS 10000
#define USECS_BETWEEN_WRITES  500

#ifdef SIGSAFE_NO_SIGINFO
void sigusr1handler(int signo, int code, struct sigcontext *ctx, intptr_t d) {
#else
void sigusr1handler(int signo, siginfo_t *si, ucontext_t *ctx, intptr_t baton) {
#endif
    write(1, ".", 1);
}

double rand_uniform(void) { return (double) random() / (double) RAND_MAX; }
double rand_exponential(double mean) { return -mean*log(rand_uniform()); }

int main(void) {
    int parent_pid;
    int mypipe[2];
    char buffer[SINGLE_TRANSFER];
    size_t total_sent = 0, total_rcvd = 0;

    srandom(time(NULL));

    sigsafe_install_handler(SIGUSR1, &sigusr1handler);
    sigsafe_install_tsd(0, NULL);

    parent_pid = getpid();
    pipe(mypipe);

    if (fork() == 0) { /* writer */
        while (total_sent < BYTES_TO_TRANSFER) {
            ssize_t retval;
            retval = write(mypipe[WRITE], buffer,
                           min(SINGLE_TRANSFER, BYTES_TO_TRANSFER - total_sent));
            assert(retval > 0);
            total_sent += retval;
            usleep(rand_exponential(USECS_BETWEEN_WRITES));
        }
        return 0;
    }

    if (fork() == 0) { /* signaler */
        while (1) {
            if (kill(parent_pid, SIGUSR1) < 0) {
                printf("Signaler ending.\n");
                exit(0);
            }
            usleep(rand_exponential(USECS_BETWEEN_SIGNALS));
        }
        /* not reached */
    }

    /* reader */
    while (total_rcvd < BYTES_TO_TRANSFER) {
        ssize_t retval;
        size_t this_transfer = min(SINGLE_TRANSFER, BYTES_TO_TRANSFER - total_rcvd);
        retval = sigsafe_read(mypipe[READ], buffer, this_transfer);
        if (retval == -EINTR) {
            sigsafe_clear_received();
            continue;
        }
        assert(retval > 0);
        if (retval == this_transfer)
            write(1, "#", 1);
        else {
            printf("[%ld]", (long) retval);
            fflush(stdout);
        }
        total_rcvd += retval;
    }
    return 0;
}
