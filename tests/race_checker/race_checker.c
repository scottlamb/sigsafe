/** @file
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
 * This file is part of sigsafe, which is released under the MIT license.
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

enum test_result {
    NOT_RUN,
    SUCCESS,
    FAILURE
};

struct test {
    const char *name;
    void* (*pre_fork_setup)();
    void (*child_setup)(void*);
    enum run_result (*instrumented)(void*);
    void (*nudge)(void*);
    void (*teardown)(void*);
    enum test_result result;
    enum test_result expected;
} tests[] = {
    { "safe_read",      &create_pipe,       &install_safe,      &do_safe_read,      &nudge_read,    &cleanup_pipe, NOT_RUN, SUCCESS },
    { "unsafe_read",    &create_pipe,       &install_unsafe,    &do_unsafe_read,    &nudge_read,    &cleanup_pipe, NOT_RUN, FAILURE},
    { NULL,             NULL,               NULL,               NULL,               NULL,           NULL, NOT_RUN, NOT_RUN }
};

/**
 * Ensures the <tt>SIGCHLD</tt> for an event has arrived.
 * @pre <i>Before</i> the event which could cause a <tt>SIGCHLD</tt>, you
 * previously ensured all other <tt>SIGCHLD</tt>s have been delivered and that
 * <tt>statuscode</tt> has been set to -1.
 * @post <tt>statuscode</tt> != -1
 */
void ensure_chld_arrived(void) {
    sigsetjmp(env, 1);
    jump_is_safe = 1;
    while (statuscode == -1)
        pause();
    jump_is_safe = 0;
}

/**
 * Runs the test for a given function.
 * @bug There is some <i>ugly</i> code in here.
 */
enum test_result run_test(const struct test *t) {
    void *test_data;
    pid_t childpid;
    int num_steps_before_continue = -1, num_steps_so_far;
    int nudge_step = -1;
    int status;
    struct timespec timeout;
    int retval;

    timeout.tv_sec = 1L;
    timeout.tv_nsec = 0L;

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
        ensure_chld_arrived();
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
            statuscode = -1;
            trace_step(childpid, 0);
            if (nudge_step == num_steps_so_far) {
                t->nudge(test_data);
            }
            sigsetjmp(env, 1);
            jump_is_safe = 1;
            while (statuscode == -1) {
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
                    kill(childpid, SIGKILL);
                    ensure_chld_arrived();
                    /*
                     * XXX could actually be two SIGCHLD events. Ugh.
                     */
                    assert(statuscode == CLD_KILLED);
                    t->teardown(test_data);
                    return FAILURE;
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
                    t->teardown(test_data);
                    return FAILURE;
                }
                break;
            } else if (statuscode == CLD_KILLED || statuscode == CLD_DUMPED) {
                printf("ERROR: Child was killed/dumped from signal.\n");
                t->teardown(test_data);
                return FAILURE;
            } else if (status == CLD_STOPPED) {
                printf("ERROR: Child was stopped?!?\n");
                t->teardown(test_data);
                return FAILURE;
            }
        }

        if (statuscode == CLD_TRAPPED) {
            printf("Continuing until exit\n");
            statuscode = -1;
            trace_detach(childpid, SIGUSR1);
            sigsetjmp(env, 1);
            jump_is_safe = 1;
            while (statuscode == -1) {
                retval = nanosleep(&timeout, NULL);
                jump_is_safe = 0;
                error_wrap(retval, "nanosleep", ERRNO);

                /* Timeout */
                kill(childpid, SIGKILL);
                printf("ERROR: timed out after continue\n");
                /* XXX Again, could be two events */
                ensure_chld_arrived();
                /* XXX should probably waitpid */
                t->teardown(test_data);
                return FAILURE;
            }
            jump_is_safe = 0;
            error_wrap(waitpid(childpid, &status, 0), "waitpid", ERRNO);
            if (WIFEXITED(status)) {
                if (WEXITSTATUS(status) == INTERRUPTED
                    && num_steps_before_continue > nudge_step) {
                    printf("ERROR: Interrupted after nudge\n");
                    t->teardown(test_data);
                    return FAILURE;
                } else if (WEXITSTATUS(status) == NORMAL
                           && num_steps_before_continue < nudge_step) {
                    printf("ERROR: normal return before nudge\n");
                    t->teardown(test_data);
                    return FAILURE;
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
    return SUCCESS;
}

int main(void) {
    struct sigaction sa;
    int i;

    sa.sa_sigaction = &sigchld_handler;
    error_wrap(sigemptyset(&sa.sa_mask), "sigempty", ERRNO);
    sa.sa_flags = SA_SIGINFO;
    error_wrap(sigaction(SIGCHLD, &sa, NULL), "sigaction", ERRNO);
    for (i = 0; tests[i].name != NULL; i++) {
        tests[i].result = run_test(&tests[i]);
    }

    printf("\n\n\n\n\n");
    for (i = 0; tests[i].name != NULL; i++) {
        printf("%20s %s\n", tests[i].name, tests[i].result == SUCCESS
                                           ? "success" : "failure");
    }

    return 0;
}
