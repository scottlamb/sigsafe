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
#include <sys/wait.h>
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

    printf("Running %s test\n", t->name);
    test_data = t->pre_fork_setup();
    if ((childpid = error_wrap(fork(), "fork", ERRNO)) == 0) {
        /* Child */
        if (t->child_setup != NULL) {
            t->child_setup(test_data);
        }
        raise(SIGSTOP); /* a marker for the parent to trace us with */
        exit((int) t->instrumented(test_data));
    }
    error_wrap(waitpid(childpid, &status, WUNTRACED), "waitpid", ERRNO);
    assert(WIFSTOPPED(status));
    trace_attach(childpid);
    while (1) {
        printf("instruction\n");
        trace_step(childpid);
        error_wrap(waitpid(childpid, &status, WUNTRACED), "waitpid", ERRNO);
        if (WIFEXITED(status)) {
            printf("Child exited.\n");
            break;
        } else {
            assert(WIFSTOPPED(status));
        }
    }
}

int main(void) {
    run_test(&tests[0]);
    return 0;
}
