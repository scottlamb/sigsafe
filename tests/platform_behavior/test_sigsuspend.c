/**
 * @file
 * Tests if <tt>sigsuspend(2)</tt> correctly returns for an already-pending
 * blocked signal.
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
#include <assert.h>

int error_wrap(int retval, const char *funcname) {
    if (retval < 0) {
        fprintf(stderr, "%s returned %d (errno==%d) (%s)\n",
                funcname, retval, errno, strerror(errno));
        abort();
    }
    return retval;
}

volatile sig_atomic_t sigusr1_received, sigalrm_received;

void sigusr1_handler(int signum) { sigusr1_received = 1; }
void sigalrm_handler(int signum) { sigalrm_received = 1; }

int main(void) {
    sigset_t usr1_set;
    sigset_t empty;
    struct timespec ts;
    struct sigaction sa;
    sigset_t pending;
    int retval;

    /* Handle and block signals */
    error_wrap(sigemptyset(&sa.sa_mask), "sigemptyset");
    error_wrap(sigaddset(&sa.sa_mask, SIGUSR1), "sigaddset");
    error_wrap(sigaddset(&sa.sa_mask, SIGALRM), "sigaddset");
    sa.sa_flags = 0;
    sa.sa_handler = &sigusr1_handler;
    error_wrap(sigaction(SIGUSR1, &sa, NULL), "sigaction");
    sa.sa_handler = &sigalrm_handler;
    error_wrap(sigaction(SIGALRM, &sa, NULL), "sigaction");
    error_wrap(sigprocmask(SIG_BLOCK, &sa.sa_mask, NULL), "sigprocmask");

    /* Generate a SIGUSR1 immediately. */
    raise(SIGUSR1);

    /* Generate a SIGALRM after about a second. */
    alarm(1);

    error_wrap(sigpending(&pending), "sigpending");
    assert( sigismember(&pending, SIGUSR1));
    assert(!sigismember(&pending, SIGALRM));

    error_wrap(sigemptyset(&empty), "sigemptyset");
    retval = sigsuspend(&empty);
    if (sigusr1_received && !sigalrm_received) {
        printf("Received the SIGUSR1 first; good.\n");
        return 0;
    } else if (sigalrm_received) {
        printf("Pending SIGUSR1 was discarded; bad.\n");
    } else {
        printf("Received both signals? weird.\n");
    }
    return 1;
}
