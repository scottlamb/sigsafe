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
 * This code is (unfortunately) a case study of where sigsafe would be
 * helpful. My signal handling is fugly and probably wrong. But I'd prefer not
 * to use the code being tested.
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
    int should_run;
} tests[] = {
    {
        name:               "sigsafe_read",
        pre_fork_setup:     &create_pipe,
        child_setup:        &install_safe,
        instrumented:       &do_sigsafe_read,
        nudge:              &nudge_read,
        teardown:           &cleanup_pipe,
        result:             NOT_RUN,
        expected:           SUCCESS
    },
    {
        name:               "racebefore_read",
        pre_fork_setup:     &create_pipe,
        child_setup:        &install_unsafe,
        instrumented:       &do_racebefore_read,
        nudge:              &nudge_read,
        teardown:           &cleanup_pipe,
        result:             NOT_RUN,
        expected:           SUCCESS
    },
    {
        name:               "racebefore_read",
        pre_fork_setup:     &create_pipe,
        child_setup:        &install_unsafe,
        instrumented:       &do_racebefore_read,
        nudge:              &nudge_read,
        teardown:           &cleanup_pipe,
        result:             NOT_RUN,
        expected:           SUCCESS
    },
    {
        name:               "raceafter_read",
        pre_fork_setup:     &create_pipe,
        child_setup:        &install_unsafe,
        instrumented:       &do_raceafter_read,
        nudge:              &nudge_read,
        teardown:           &cleanup_pipe,
        result:             NOT_RUN,
        expected:           SUCCESS
    },
    {
        name:               NULL,
        pre_fork_setup:     NULL,
        child_setup:        NULL,
        instrumented:       NULL,
        nudge:              NULL,
        teardown:           NULL,
        result:             NOT_RUN,
        expected:           NOT_RUN
    }
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
 * Takes care of getting rid of a child that has timed out and making sure
 * the signal has arrived.
 */
void smite_child(pid_t childpid) {
    kill(childpid, SIGKILL);
    /*
     * One would assume that after waitpid(), the SIGCHLD has arrived.
     */
    error_wrap(waitpid(childpid, NULL, 0), "waitpid", ERRNO);
    assert(statuscode == CLD_KILLED);
}

/**
 * Runs the test for a given function.
 * @bug There is some <i>ugly</i> code in here.
 */
enum test_result run_test(const struct test *t) {
    void *test_data;
    pid_t childpid;
    /*
     * XXX
     * For some reason, exit() takes a _LOT_ of instructions - a lot of atexit
     * handlers? I just assume for now that nothing exciting will happen after
     * 1000 instructions so I don't have to wait forever.
     */
    int num_steps_before_continue = 1000/*-1*/,
        num_steps_so_far;
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

        if (num_steps_before_continue != -1) {
            printf("%d ", num_steps_before_continue);
            fflush(stdout);
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
                    printf("\nERROR: timeout on step %d\n\n",
                           num_steps_so_far);
                    smite_child(childpid);
                    t->teardown(test_data);
                    return FAILURE;
                }
                jump_is_safe = 1;
            }
            jump_is_safe = 0;
            if (statuscode == CLD_EXITED) {
                assert(num_steps_before_continue == -1);
                error_wrap(waitpid(childpid, &status, 0), "waitpid", ERRNO);
                if (WEXITSTATUS(status) != NORMAL) {
                    fprintf(stderr,
                            "\nERROR: First run should be a normal exit.\n\n");
                    t->teardown(test_data);
                    return FAILURE;
                }
                break;
            } else if (statuscode == CLD_KILLED || statuscode == CLD_DUMPED) {
                printf("\nERROR: Child was killed/dumped from signal.\n\n");
                t->teardown(test_data);
                return FAILURE;
            } else if (status == CLD_STOPPED) {
                printf("\nERROR: Child was stopped?!?\n\n");
                t->teardown(test_data);
                return FAILURE;
            }
        }

        if (statuscode == CLD_TRAPPED) {
            statuscode = -1;
            trace_detach(childpid, SIGUSR1);
            sigsetjmp(env, 1);
            jump_is_safe = 1;
            while (statuscode == -1) {
                retval = nanosleep(&timeout, NULL);
                jump_is_safe = 0;
                error_wrap(retval, "nanosleep", ERRNO);

                /* Timeout */
                smite_child(childpid);
                printf("\nERROR: timed out after continue\n\n");
                t->teardown(test_data);
                return FAILURE;
            }
            jump_is_safe = 0;
            error_wrap(waitpid(childpid, &status, 0), "waitpid", ERRNO);
            if (WIFEXITED(status)) {
                if (WEXITSTATUS(status) == INTERRUPTED
                    && num_steps_before_continue > nudge_step) {
                    printf("\nERROR: Interrupted after nudge\n\n");
                    t->teardown(test_data);
                    return FAILURE;
                } else if (WEXITSTATUS(status) == NORMAL
                           && num_steps_before_continue < nudge_step) {
                    printf("\nERROR: normal return before nudge\n\n");
                    t->teardown(test_data);
                    return FAILURE;
                } else if (WEXITSTATUS(status) == INTERRUPTED) {
                    /*printf("\nGood; interrupted\n");*/
                } else if (WEXITSTATUS(status) == NORMAL) {
                    /*printf("\nGood; normal\n");*/
                } else {
                    abort();
                }
            } else if (WIFSIGNALED(status)) {
                printf("\nERROR: exited on signal %d.\n\n", WTERMSIG(status));
            } else {
                abort();
            }
        }
        num_steps_before_continue = num_steps_so_far - 1;
        t->teardown(test_data);
    } while (num_steps_before_continue >= 0) ;
    printf("\nSuccess\n\n");
    return SUCCESS;
}

/** Prints a usage message to stdout. */
void help(void) {
    printf("race_checker - exhaustively search for race conditions in signal code.\n\n");
    printf("Usage:\n\n");

    printf("race_checker -h\n");
    printf("race_checker --help\n");
    printf("    Prints this message and exits.\n\n");

    printf("race_checker -l\n");
    printf("race_checker --list\n");
    printf("    Lists the available tests.\n\n");

    printf("race_checker -a\n");
    printf("race_checker --run-all\n");
    printf("    Runs all tests.\n\n");

    printf("race_checker test1 [test2 [test3 [...]]]\n");
    printf("    Runs the specified tests only.\n\n");
}

int main(int argc, char **argv) {
    struct sigaction sa;
    int i, run_all = 0, run_specific = 0;

    sa.sa_sigaction = &sigchld_handler;
    error_wrap(sigemptyset(&sa.sa_mask), "sigempty", ERRNO);
    sa.sa_flags = SA_SIGINFO;
    error_wrap(sigaction(SIGCHLD, &sa, NULL), "sigaction", ERRNO);

    while (argc > 1) {
        --argc; ++argv;
        if (strcmp(*argv, "--list") == 0 || strcmp(*argv, "-l") == 0) {
            /* List the tests we can run */
            printf("%-20s Expected result\n", "Test name");
            for (i = 0; tests[i].name != NULL; i++) {
                printf("%-20s %s\n",
                       tests[i].name,
                       tests[i].expected == SUCCESS ? "success"
                                                    : "failure");
            }
            return 0;
        } else if (   strcmp(*argv, "--run-all") == 0
                   || strcmp(*argv, "-a") == 0) {
            run_all = 1;
        } else if ((*argv)[0] != 0 && (*argv)[0] != '-') {
            /* A specific test to run */
            run_specific = 1;
            for (i = 0; tests[i].name != NULL; i++) {
                if (strcmp(*argv, tests[i].name) == 0) {
                    tests[i].should_run = 1;
                    break;
                }
            }
            if (tests[i].name == NULL) {
                fprintf(stderr, "Couldn't find test '%s'\n", *argv);
                return 1;
            }
        } else if (strcmp(*argv, "-h") == 0 || strcmp(*argv, "--help") == 0) {
            help();
            return 0;
        } else {
            fprintf(stderr, "Invalid arguments.\n");
            help();
            return 0;
        }
    }

    if (!run_all && !run_specific) {
        fprintf(stderr, "No tests given.\n");
        help();
        return 1;
    }

    /* Run all tests */
    for (i = 0; tests[i].name != NULL; i++) {
        if (tests[i].should_run || run_all) {
            tests[i].result = run_test(&tests[i]);
        }
    }

    printf("\n\n\n\n\n");
    for (i = 0; tests[i].name != NULL; i++) {
        if (tests[i].result != NOT_RUN) {
            printf("%20s %s\n", tests[i].name, tests[i].result == SUCCESS
                                               ? "success" : "failure");
        }
    }

    return 0;
}
