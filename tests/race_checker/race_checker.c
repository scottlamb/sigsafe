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

void run_test(struct test *t) {
    void *test_data;
    pid_t childpid;
    int status;
    int num_steps_before_continue = 500/*-1*/, num_steps_so_far;
    int nudge_step = -1;
    sigset_t chld_set;
    struct siginfo info;
    struct timespec timeout;
    int retval;

    timeout.tv_sec = 1L;
    timeout.tv_nsec = 0L;

    error_wrap(sigemptyset(&chld_set), "sigemptyset", ERRNO);
    error_wrap(sigaddset(&chld_set, SIGCHLD), "sigaddset", ERRNO);
    error_wrap(pthread_sigmask(SIG_SETMASK, &chld_set, NULL),
               "pthread_sigmask", DIRECT);

    printf("Running %s test\n", t->name);
    do {
        printf("Iteration\n");
        test_data = t->pre_fork_setup();
        if ((childpid = error_wrap(fork(), "fork", ERRNO)) == 0) {
            /* Child */
            if (t->child_setup != NULL) {
                t->child_setup(test_data);
            }
            raise(SIGSTOP); /* a marker for the parent to trace us with */
            exit((int) t->instrumented(test_data));
        }
        retval = sigwaitinfo(&chld_set, &info);
        assert(retval == SIGCHLD);
        assert(info.si_code == CLD_STOPPED);
        trace_attach(childpid);

        if (num_steps_before_continue == -1) {
            printf("Stepping until exit\n");
        } else {
            printf("Stepping %d instructions\n", num_steps_before_continue);
        }
        printf("    ");
        for (num_steps_so_far = 0;
                num_steps_before_continue == -1
             || num_steps_so_far < num_steps_before_continue;
             num_steps_so_far++) {
            /*printf("(step)"); fflush(stdout);*/
            trace_step(childpid);
            while (1) {
                if ((retval = sigtimedwait(&chld_set, &info, &timeout))
                    != SIGCHLD) {
                    if (retval == -1 && errno == EAGAIN) {
                        if (nudge_step == -1) {
                            printf("Nudge at instruction %d\n", num_steps_so_far);
                            nudge_step = num_steps_so_far;
                        } else if (nudge_step != num_steps_so_far) {
                            printf("WARNING: nudge step different! (new %d, old %d)\n",
                                   num_steps_so_far, nudge_step);
                        } else {
                            printf("(nudge)");
                            fflush(stdout);
                        }
                        t->nudge(test_data);
                    } else if (retval == -1) {
                        error_wrap(retval, "sigtimedwait", ERRNO);
                    } else {
                        fprintf(stderr, "Received signal %d\n", retval);
                        abort();
                    }
                } else if (info.si_code == CLD_CONTINUED) {
                    printf("(continued)");
                    fflush(stdout);
                } else {
                    break;
                }
            }
            if (info.si_code == CLD_EXITED) {
                assert(num_steps_before_continue == -1);
                printf("Child exited with status %d\n", WEXITSTATUS(status));
                /* FIXME */
                break;
            } else if (info.si_code == CLD_TRAPPED) {
                break;
            } else if (info.si_code == CLD_KILLED
                       || info.si_code == CLD_DUMPED) {
                printf("Umm. It died. That's bad.\n");
            } else if (info.si_code == CLD_STOPPED) {
                printf("It stopped?!?\n");
            } else {
                printf("Unknown CLD si_code\n");
                abort();
            }
        }
        printf("\n");

        if (WIFSTOPPED(status)) {
            printf("Continuing until exit\n");
            trace_detach(childpid, SIGUSR1);
            error_wrap(waitpid(childpid, &status, 0), "waitpid", ERRNO);
            if (WIFEXITED(status)) {
                printf("Exited with value %d.\n", WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("Exited on signal %d.\n", WTERMSIG(status));
            } else {
                abort();
            }
        }
        num_steps_before_continue = num_steps_so_far - 1;
        t->teardown(test_data);
    } while (num_steps_before_continue >= 0) ;
}

int main(void) {
    run_test(&tests[0]);
    return 0;
}
