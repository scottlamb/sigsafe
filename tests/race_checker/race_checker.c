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
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

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
    void* (*setup)();
    void (*instrumented)(void*);
    void (*nudge)(void*);
    void (*teardown)(void*);
} tests[] = {
    { "read",   &create_pipe, &do_read, &nudge_read, &cleanup_pipe },
    { NULL,     NULL,         NULL,      NULL,       NULL }
};

void run_test(struct test *t) {
    void *test_data;
    pid_t childpid;
    int status;

    printf("Running %s test\n", t->name);
    test_data = t->setup();
    if (childpid = error_wrap(fork(), "fork", ERRNO) == 0) {
        /* Child */
        raise(SIGSTOP); /* a marker for the parent to trace us with */
        exit(test->instrumented(test_data));
    }
    error_wrap(waitpid(childpid, &status, WUNTRACED), "waitpid", ERRNO);
    assert(WIFSTOPPED(status));
    error_wrap(ptrace(PTRACE_ATTACH, childpid, 0, 0), "ptrace", ERRNO);
}

int main(void) {
    run_test(&tests[0]);
    return 0;
}
