/** @file
 * Tests if signal delivery can cause us to lose track of received bytes on
 * sockets.
 * See test_pipe_bytecount for a rough description. Here we test with
 * SO_RCVLOWAT. The SUSv3 specification for which reads:
 *
 *     "If SO_RCVLOWAT is set to a larger value, blocking receive calls
 *     normally wait until they have received the smaller of the low water
 *     mark value or the requested amount. (They may return less than the low
 *     water mark if [...] a signal is caught [...])"
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
} Half;

double rand_uniform() { return (double) random() / (double) RAND_MAX; }
double rand_exponential(double mean) { return -mean * log(rand_uniform()); }

#define USECS_BETWEEN_SIGNALS 10000
#define USECS_BETWEEN_WRITES 500
#define BYTES_TO_TRANSFER 268435456/*4294967295u*/
#define SINGLE_WRITE PIPE_BUF
#define SINGLE_READ (4*SINGLE_WRITE)

enum error_return_type {
    NEGATIVE,
    ERRNO
};

int error_wrap(int retval, const char *funcname, enum error_return_type type) {
    if (type == ERRNO && retval < 0) {
        fprintf(stderr, "%s returned %d (errno==%d) (%s)\n",
                funcname, retval, errno, strerror(errno));
        abort();
    } else if (type == NEGATIVE && retval < 0) {
        fprintf(stderr, "%s returned %d (%s)\n",
                funcname, retval, strerror(-retval));
        abort();
    }
    return retval;
}

void sigusr1handler(int signo, siginfo_t *si, ucontext_t *ctx, intptr_t baton) {
    write(1, ".", 1);
}

int main(void) {
    int parent_pid;
    int sockets[2];
    char buffer[SINGLE_READ];
    size_t total_sent = 0, total_rcvd = 0;
    size_t lowat;

    srandom(time(NULL));

    sigsafe_install_handler(SIGUSR1, &sigusr1handler);
    sigsafe_install_tsd(0, NULL);

    parent_pid = getpid();
    error_wrap(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets),
               "socketpair", ERRNO);

    lowat = SINGLE_READ;
    error_wrap(setsockopt(sockets[READ], SOL_SOCKET, SO_RCVLOWAT, &lowat,
                          sizeof(lowat)), "setsockopt", ERRNO);

    if (fork() == 0) { /* writer */
        while (total_sent < BYTES_TO_TRANSFER) {
            ssize_t retval;
            retval = write(sockets[WRITE], buffer,
                           min(SINGLE_WRITE, BYTES_TO_TRANSFER - total_sent));
            error_wrap(retval, "write", ERRNO);
            total_sent += retval;
            usleep(rand_exponential(USECS_BETWEEN_WRITES));
        }
        return 0;
    }

    if (fork() == 0) { /* signaler */
        while (1) {
            usleep(rand_exponential(USECS_BETWEEN_SIGNALS));
            if (kill(parent_pid, SIGUSR1) < 0) {
                printf("Signaler ending.\n");
                exit(0);
            }
        }
        /* not reached */
    }

    /* reader */
    while (total_rcvd < BYTES_TO_TRANSFER) {
        ssize_t retval;
        size_t this_transfer = min(SINGLE_READ, BYTES_TO_TRANSFER - total_rcvd);
        sigsafe_clear_received();
        retval = sigsafe_read(sockets[READ], buffer, this_transfer);
        if (retval == -EINTR) {
            continue;
        }
        error_wrap(retval, "read", NEGATIVE);
        if (retval == this_transfer)
            write(1, "#", 1);
        else {
            printf("[%d]", retval);
            fflush(stdout);
        }
        total_rcvd += retval;
    }
    return 0;
}
