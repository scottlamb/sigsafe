/**
 * Tests system call signal handling for race conditions.
 * This runs the race in every possible way &mdash; for each system call,
 * it uses the system tracing API to test the behavior at each
 * instruction. It looks for:
 *
 * - an initial section in which a signal causes immediate <tt>EINTR</tt>
 *   return without another event causing the system call to complete.
 *
 * - a subsequent section in which a signal has no effect, and
 *   another event causes normal return with the result of the system call.
 *
 * In particular, there must be no sections for which either of the following
 * is true:
 *
 * - a signal is received but the code suspends pending the result of the
 *   system call.
 *
 * - the system call completes normally and then the return is lost due to
 *   the signal handling.
 *
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is released under the MIT license.
 * @version         $Id$
 * @author          Scott Lamb &lt;slamb@slamb.org&gt;
 */

#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <setjmp.h>
#include "race_checker.h"

enum test_result {
    CANCELED,
    COMPLETED
};

int error_wrap(int retval, const char *funcname, enum error_return_type type) {
    if (type == ERRNO && retval < 0) {
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        fprintf(stderr, "%s returned %d (errno==%d) (%s)\n",
                funcname, retval, errno, strerror(errno));
        abort();
    } else if (type == DIRECT && retval != 0) {
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        fprintf(stderr, "%s returned %d (%s)\n",
                funcname, retval, strerror(retval));
        abort();
    } else if (type == NEGATIVE && retval < 0) {
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        fprintf(stderr, "%s returned %d (%s)\n",
                funcname, retval, strerror(-retval));
        abort();
    }
    return retval;
}

volatile sig_atomic_t jump_is_safe;
volatile sig_atomic_t statuscode;
sigjmp_buf env;

void sigchld_handler(int signum, siginfo_t *info, void *ctx) {
    assert(signum == SIGCHLD);
    /*write(2, "CHLD ", 5);*/
    if (info->si_code == CLD_CONTINUED) {
        /* ignored; we know this */
    } else {
        statuscode = info->si_code;
        if (jump_is_safe) {
            jump_is_safe = 0;
            siglongjmp(env, 1);
        }
    }
}

struct test {
    const char *name;
    void* (*pre_fork_setup)();
    void (*child_setup)(void*);
    enum run_result (*instrumented)(void*);
    void (*nudge)(void*);
    void (*teardown)(void*);
} tests[] = {
    { "safe_read",      &create_pipe,       &install_safe,      &do_safe_read,      &nudge_read,    &cleanup_pipe },
    { "unsafe_read",    &create_pipe,       &install_unsafe,    &do_unsafe_read,    &nudge_read,    &cleanup_pipe },
    { NULL,             NULL,               NULL,               NULL,               NULL,           NULL }
};

/**
 * Run the test for a given function.
 * @bug There is some <i>ugly</i> code in here.
 */
void run_test(struct test *t) {
    void *test_data;
    pid_t childpid;
    int num_steps_before_continue = -1, num_steps_so_far;
    int nudge_step = -1;
    int status;
    struct timespec timeout;
    int retval;

    printf("Running %s test\n", t->name);
    do {
        test_data = t->pre_fork_setup();
        statuscode = -1;
        if ((childpid = error_wrap(fork(), "fork", ERRNO)) == 0) {
            /* Child */
            if (t->child_setup != NULL) {
                t->child_setup(test_data);
            }
            raise(SIGSTOP); /* a marker for the parent to trace us with */
            exit((int) t->instrumented(test_data));
        }
        sigsetjmp(env, 1);
        jump_is_safe = 1;
        while (statuscode == -1) pause();
        jump_is_safe = 0;
        assert(statuscode == CLD_STOPPED);
        trace_attach(childpid);

        if (num_steps_before_continue == -1) {
            printf("Stepping until exit\n");
        } else {
            printf("Stepping %d instructions\n", num_steps_before_continue);
        }
        for (num_steps_so_far = 0;
                num_steps_before_continue == -1
             || num_steps_so_far < num_steps_before_continue;
             num_steps_so_far++) {
            /*printf("(step)"); fflush(stdout);*/
            statuscode = -1;
            trace_step(childpid, num_steps_so_far == nudge_step ? SIGUSR1
                                                                : 0);
            sigsetjmp(env, 1);
            jump_is_safe = 1;
            while (statuscode == -1) {
                timeout.tv_sec = 1L;
                timeout.tv_nsec = 0L;
                retval = nanosleep(&timeout, NULL);
                jump_is_safe = 0;
                error_wrap(retval, "nanosleep", ERRNO);

                /* Timed out; we need to nudge it */
                if (nudge_step == -1) {
                    printf("Nudge at instruction %d\n", num_steps_so_far);
                    nudge_step = num_steps_so_far;
                    t->nudge(test_data);
                } else {
                    printf("ERROR: timeout on step %d\n",
                           num_steps_so_far);
                }
                jump_is_safe = 1;
            }
            jump_is_safe = 0;
            if (statuscode == CLD_EXITED) {
                assert(num_steps_before_continue == -1);
                error_wrap(waitpid(childpid, &status, 0), "waitpid", ERRNO);
                printf("Child exited with status %d\n", WEXITSTATUS(status));
                if (WEXITSTATUS(status) != NORMAL) {
                    fprintf(stderr, "ERROR: First run should be a normal exit.\n");
                }
                break;
            } else if (statuscode == CLD_KILLED || statuscode == CLD_DUMPED) {
                printf("ERROR: Child was killed/dumped from signal.\n");
            } else if (status == CLD_STOPPED) {
                printf("ERROR: Child was stopped?!?\n");
            }
        }

        if (statuscode == CLD_TRAPPED) {
            printf("Continuing until exit\n");
            trace_detach(childpid, SIGUSR1);
            error_wrap(waitpid(childpid, &status, 0), "waitpid", ERRNO);
            if (WIFEXITED(status)) {
                if (WEXITSTATUS(status) == INTERRUPTED
                    && num_steps_before_continue >= nudge_step) {
                    printf("ERROR: Interrupted after nudge\n");
                } else if (WEXITSTATUS(status) == NORMAL
                           && num_steps_before_continue < nudge_step) {
                    printf("ERROR: normal return before nudge\n");
                } else if (WEXITSTATUS(status) == INTERRUPTED) {
                    printf("Good; interrupted\n");
                } else if (WEXITSTATUS(status) == NORMAL) {
                    printf("Good; normal\n");
                } else {
                    abort();
                }
            } else if (WIFSIGNALED(status)) {
                printf("ERROR: exited on signal %d.\n", WTERMSIG(status));
            } else {
                abort();
            }
        }
        num_steps_before_continue = num_steps_so_far - 1;
        t->teardown(test_data);
        printf("\n");
    } while (num_steps_before_continue >= 0) ;
}

int main(void) {
    struct sigaction sa;
    int i;

    sa.sa_sigaction = &sigchld_handler;
    error_wrap(sigemptyset(&sa.sa_mask), "sigempty", ERRNO);
    sa.sa_flags = SA_SIGINFO;
    error_wrap(sigaction(SIGCHLD, &sa, NULL), "sigaction", ERRNO);
    for (i = 0; tests[i]->name != NULL; i++) {
        run_test(&tests[i]);
    }
    return 0;
}
