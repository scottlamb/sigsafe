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

#define QUICK_OFFSET_AFTER  25
#define QUICK_OFFSET_BEFORE 25

int quick_mode;

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

enum test_result {
    NOT_RUN = 0,
    SUCCESS,
    FAILURE,
    FORGOTTEN_RESULT,
    IGNORED_SIGNAL
};

const char *labels[] = {
    "not run",
    "success",
    "misc. failure",
    "forgotten result",
    "ignored signal"
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
    int in_most;
    int should_run;
} tests[] = {
    {
        /*
         * Tests that signal delivery is safe before, during, or after
         * the sigsafe_install_handler / sigsafe_install_tsd sequence.
         */
        name:               "install_safe",
        pre_fork_setup:     NULL,
        child_setup:        /* Effectively ignoring the signal so the SIGUSR1
                               doesn't cause it to exit on signal if delivered
                               before the install_sighandler */
                            &install_unsafe,
        instrumented:       &do_install_safe,
        nudge:              NULL,
        teardown:           NULL,
        result:             NOT_RUN,
        expected:           SUCCESS,
        in_most:            0 /* this test is _slow_ */
    },
    /*
     * XXX should have a test for dyld deadlock on Darwin
     * Ensure it fails when the workaround code in sigsafe.c is removed.
     */
    {
        name:               "sigsafe_read",
        pre_fork_setup:     &create_pipe,
        child_setup:        &install_safe,
        instrumented:       &do_sigsafe_read,
        nudge:              &nudge_read,
        teardown:           &cleanup_pipe,
        result:             NOT_RUN,
        expected:           SUCCESS,
        in_most:            1
    },
    {
        name:               "racebefore_read",
        pre_fork_setup:     &create_pipe,
        child_setup:        &install_unsafe,
        instrumented:       &do_racebefore_read,
        nudge:              &nudge_read,
        teardown:           &cleanup_pipe,
        result:             NOT_RUN,
        expected:           IGNORED_SIGNAL,
        in_most:            1
    },
    {
        name:               "raceafter_read",
        pre_fork_setup:     &create_pipe,
        child_setup:        &install_unsafe,
        instrumented:       &do_raceafter_read,
        nudge:              &nudge_read,
        teardown:           &cleanup_pipe,
        result:             NOT_RUN,
        expected:           FORGOTTEN_RESULT,
        in_most:            1
    },
    {
        name:               NULL,
        pre_fork_setup:     NULL,
        child_setup:        NULL,
        instrumented:       NULL,
        nudge:              NULL,
        teardown:           NULL,
        result:             NOT_RUN,
        expected:           NOT_RUN,
        in_most:            0
    }
};

/**
 * @group sighandling Reliably wait for <tt>SIGCHLD</tt> or <tt>SIGALRM</tt>
 * signals.
 * These functions are rather complex because I did not want to use
 * <tt>sigsafe</tt> in the tester. Also because of portability - it would have
 * been easier to use <tt>sigtimedwait(2)</tt>, but the realtime signal
 * extensions are not available on Darwin.
 */
/*@{*/

enum event {
    EVENT_NONE,
    EVENT_SIGCHLD,
    EVENT_TIMEOUT
};

siginfo_t *wait_for_sigchld_info;
enum event wait_for_sigchld_event;

sigset_t chld_alrm_set;

/**
 * Signal handler for wait_for_sigchld.
 * As this is only called during sigsuspend, it can safely access data
 * structures other than <tt>volatile sig_atomic_t</tt> ones.
 */
void wait_for_sigchld_helper(int signum, siginfo_t *info, void *ctx) {
    if (wait_for_sigchld_event != EVENT_NONE) {
        abort();
    }
    if (signum == SIGCHLD) {
        memcpy(wait_for_sigchld_info, info, sizeof(siginfo_t));
        wait_for_sigchld_event = EVENT_SIGCHLD;
    } else if (signum == SIGALRM) {
        wait_for_sigchld_event = EVENT_TIMEOUT;
    } else {
        /* This shouldn't happen */
        abort();
    }
}

void setup_for_wait_for_sigchld(void) {
    struct sigaction sa;

    /* Block SIGCHLD and SIGALRM for most of our program's execution. */
    error_wrap(sigemptyset(&chld_alrm_set), "sigemptyset", ERRNO);
    error_wrap(sigaddset(&chld_alrm_set, SIGCHLD), "sigchld", ERRNO);
    error_wrap(sigaddset(&chld_alrm_set, SIGALRM), "sigchld", ERRNO);
    error_wrap(sigprocmask(SIG_BLOCK, &chld_alrm_set, NULL), "sigprocmask", ERRNO);

    /*
     * Installs the SIGCHLD and SIGALRM handlers necessary for
     * wait_for_sigchld. Note that they block each other during delivery.
     */
    sa.sa_sigaction = &wait_for_sigchld_helper;
    memcpy(&sa.sa_mask, &chld_alrm_set, sizeof(sigset_t));
    sa.sa_flags = SA_SIGINFO;
    error_wrap(sigaction(SIGCHLD, &sa, NULL), "sigaction", ERRNO);
    error_wrap(sigaction(SIGALRM, &sa, NULL), "sigaction", ERRNO);
}

/**
 * Waits for a <tt>SIGCHLD</tt> or an optional timeout.
 * This uses <tt>sigsuspend(2)</tt> and a timer, as Darwin does not support
 * the realtime extensions (with the friendlier <tt>sigtimedwait(2)</tt>).
 * Assumes no other signals will arrive during this handling.
 * @pre There is no active timer or pending <tt>SIGALRM</tt>.
 * @pre <tt>SIGCHLD</tt> and <tt>SIGALRM</tt> are blocked and handlers
 *      installed as by setup_for_wait_for_sigchld().
 * @post (Preconditions are again true)
 * @post <tt>wait_for_sigchld_info</tt> is NULL.
 * @post If returning with <tt>EVENT_SIGCHLD</tt>, no zombie process will
 *       exist due to this child
 * @return <tt>EVENT_SIGCHLD</tt> or <tt>EVENT_TIMEOUT</tt>, whichever
 *         occurred first.
 */
enum event wait_for_sigchld(siginfo_t *info, const struct timeval *timeout) {
    int retval;
    sigset_t no_signals;
    struct itimerval it;

    assert(info != NULL);
    if (timeout != NULL) {
        memcpy(&it.it_value, timeout, sizeof(struct timeval));
        it.it_interval.tv_sec = 0;
        it.it_interval.tv_usec = 0;
        error_wrap(setitimer(ITIMER_REAL, &it, &it), "setitimer", ERRNO);
        assert(it.it_value.tv_sec == 0 && it.it_value.tv_usec == 0);
    }

    /* Wait for exactly one signal of SIGCHLD or SIGALRM type. */
    error_wrap(sigemptyset(&no_signals), "sigemptyset", ERRNO);
    assert(   wait_for_sigchld_info == NULL
           && wait_for_sigchld_event == EVENT_NONE);
    wait_for_sigchld_info = info;
    retval = sigsuspend(&no_signals);
    assert(retval == -1 && errno == EINTR);
    wait_for_sigchld_info = NULL;
    
    if (wait_for_sigchld_event == EVENT_TIMEOUT) {
        wait_for_sigchld_event = EVENT_NONE;
        return EVENT_TIMEOUT;
    }

    wait_for_sigchld_event = EVENT_NONE;

    if (   info->si_code == CLD_EXITED || info->si_code == CLD_KILLED
        || info->si_code == CLD_DUMPED) {
        /*
         * As well as reaping the zombie, this ensures the status code is set
         * correctly on exit. That is not true otherwise on Linux. (A bug, I
         * think.)
         */
        error_wrap(waitpid(info->si_pid, &info->si_status, WNOHANG),
                   "waitpid", ERRNO);
    }

    if (timeout != NULL) {
        /*
         * Clean up the timer.
         * Disable it, _then_ check for a pending signal and clear it if
         * necessary. (Note the it itimer value is 0 from before.)
         */

        sigset_t pending_signals;
        error_wrap(setitimer(ITIMER_REAL, &it, NULL), "setitimer", ERRNO);
        error_wrap(sigpending(&pending_signals), "sigpending", ERRNO);
        if (sigismember(&pending_signals, SIGALRM)) {
            sigset_t other_signals;
            error_wrap(sigfillset(&other_signals), "sigfillset", ERRNO);
            error_wrap(sigdelset(&other_signals, SIGALRM), "sigdelset", ERRNO);
            sigsuspend(&other_signals);
            assert(   retval == -1 && errno == EINTR
                   && wait_for_sigchld_event == EVENT_TIMEOUT);
            wait_for_sigchld_event = EVENT_NONE;
        }
    }

    return EVENT_SIGCHLD;
}
//@}

/**
 * Takes care of getting rid of a child that has timed out and making sure
 * the signal has arrived.
 */
void smite_child(pid_t childpid) {
    siginfo_t info;
    int retval;

    retval = kill(childpid, SIGKILL);
    /* An ESRCH is okay; it means the child just exited on its own. */
    if (retval == -1 && errno != ESRCH) {
        error_wrap(retval, "kill", ERRNO);
    }
    wait_for_sigchld(&info, NULL);
    assert(   info.si_code == CLD_EXITED || info.si_code == CLD_KILLED
           || info.si_code == CLD_DUMPED);
}

/**
 * Runs the test for a given function.
 * @bug There is some <i>ugly</i> code in here.
 */
enum test_result run_test(const struct test *t) {
    void *test_data = NULL;
    struct timeval timeout;
    pid_t childpid;
    int num_steps_before_continue = -1,
        num_steps_so_far;
    int syscall_step = -1;
    siginfo_t info;

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    printf("Running %s test\n", t->name);
    do {
        if (t->pre_fork_setup != NULL)
            test_data = t->pre_fork_setup();
        if ((childpid = error_wrap(fork(), "fork", ERRNO)) == 0) {
            /* Child */
            if (t->child_setup != NULL) {
                t->child_setup(test_data);
            }
            raise(SIGSTOP); /* a marker for the parent to trace us with */
            /*
             * Using _exit instead of exit to trim the number of instructions
             * (no atexit handler). If this isn't available somewhere, maybe
             * I'll switch it to using raise() again instead.
             */
            _exit((int) t->instrumented(test_data));
        }
        wait_for_sigchld(&info, NULL);
        assert(info.si_code == CLD_STOPPED);
        trace_attach(childpid);

        if (num_steps_before_continue != -1) {
            printf("%d ", num_steps_before_continue);
            fflush(stdout);
        }
        for (num_steps_so_far = 0;
                num_steps_before_continue == -1
             || num_steps_so_far < num_steps_before_continue;
             num_steps_so_far++) {
            trace_step(childpid, 0);
            if (syscall_step == num_steps_so_far) {
                t->nudge(test_data);
            }
            while (wait_for_sigchld(&info, &timeout) == EVENT_TIMEOUT) {
                if (t->nudge == NULL) {
                    printf("\nERROR: Timeout on nudge-free function.\n\n");
                    smite_child(childpid);
                    if (t->teardown != NULL)
                        t->teardown(test_data);
                    return FAILURE;
                } else if (syscall_step == -1) {
                    printf("Nudge required for instruction %d to complete;"
                           " assumed to be syscall\n", num_steps_so_far+1);
                    syscall_step = num_steps_so_far;
                    t->nudge(test_data);
                    if (quick_mode) {
                        num_steps_before_continue =   num_steps_so_far
                                                   + QUICK_OFFSET_AFTER;
                    }
                } else {
                    printf("\nERROR: timeout on step %d\n\n",
                           num_steps_so_far);
                    smite_child(childpid);
                    if (t->teardown != NULL)
                        t->teardown(test_data);
                    return FAILURE;
                }
            }
            if (info.si_code == CLD_EXITED) {
                assert(num_steps_so_far > syscall_step);
                if (WEXITSTATUS(info.si_status) != NORMAL) {
                    fprintf(stderr,
                            "\nERROR: First run should be a normal exit.\n\n");
                    if (t->teardown != NULL)
                        t->teardown(test_data);
                    return FAILURE;
                }
                break;
            } else if (info.si_code == CLD_KILLED || info.si_code == CLD_DUMPED) {
                printf("\nERROR: Child was killed/dumped from signal %d.\n\n",
                       WIFSIGNALED(info.si_status));
                if (t->teardown != NULL)
                    t->teardown(test_data);
                return FAILURE;
            } else if (info.si_code == CLD_STOPPED) {
                printf("\nERROR: Child was stopped?!?\n\n");
                if (t->teardown != NULL)
                    t->teardown(test_data);
                return FAILURE;
            }
        }

        if (info.si_code == CLD_TRAPPED) {
            /*
             * We haven't gone all the way through;
             * send it a signal and continue.
             */
            trace_detach(childpid, SIGUSR1);
            if (wait_for_sigchld(&info, &timeout) == EVENT_TIMEOUT) {
                /* Timeout */
                smite_child(childpid);
                if (t->teardown != NULL)
                    t->teardown(test_data);
                if (num_steps_before_continue > syscall_step) {
                    printf("\nERROR: timed out after syscall then signal\n\n");
                    return FAILURE;
                } else if (num_steps_before_continue == syscall_step) {
                    printf("\nERROR: timed out on signal then syscall\n\n");
                    return IGNORED_SIGNAL;
                } else {
                    printf("\nERROR: timed out before syscall, after signal\n\n");
                    return FAILURE;
                }
            }
            if (info.si_code == CLD_EXITED) {
                if (WEXITSTATUS(info.si_status) == INTERRUPTED
                    && num_steps_before_continue > syscall_step) {
                    printf("\nERROR: Interrupted after syscall\n\n");
                    if (t->teardown != NULL)
                        t->teardown(test_data);
                    return FORGOTTEN_RESULT;
                } else if (WEXITSTATUS(info.si_status) == NORMAL
                           && num_steps_before_continue < syscall_step) {
                    printf("\nERROR: normal return before syscall\n\n");
                    if (t->teardown != NULL)
                        t->teardown(test_data);
                    return FAILURE;
                } else if (WEXITSTATUS(info.si_status) == INTERRUPTED) {
                    /*printf("\nGood; interrupted\n");*/
                } else if (WEXITSTATUS(info.si_status) == NORMAL) {
                    /*printf("\nGood; normal\n");*/
                } else {
                    abort();
                }
            } else if (   info.si_code == CLD_KILLED
                       || info.si_code == CLD_DUMPED) {
                printf("\nERROR: exited on signal %d.\n\n",
                       WTERMSIG(info.si_status));
            } else if (info.si_code != CLD_TRAPPED) {
                abort();
            }
        }
        num_steps_before_continue = num_steps_so_far - 1;
        if (t->teardown != NULL)
            t->teardown(test_data);
    } while (   num_steps_before_continue >= 0
             && (!quick_mode ||    num_steps_before_continue
                                >= syscall_step - QUICK_OFFSET_BEFORE)) ;

    if (syscall_step == -1 && t->nudge != NULL) {
        printf("\nERROR: No nudge ever required?!?\n\n");
        return FAILURE;
    }

    printf("\nSuccess\n\n");
    return SUCCESS;
}

/** Prints a usage message to stdout. */
void help(void) {
    printf("race_checker - exhaustively search for race conditions in signal code.\n\n");
    printf("Usage:\n\n");

    printf("\trace_checker <-h | --help>\n");
    printf("\t    Prints this message and exits.\n\n");

    printf("\trace_checker <-l | --list>\n");
    printf("\t    Lists the available tests.\n\n");

    printf("\trace_checker [-q | --quick] <-m | --run-most>\n");
    printf("\t    Runs most tests (all but the really slow ones).\n\n");

    printf("\trace_checker [-q | --quick] <-a | --run-all>\n");
    printf("\t    Runs all tests.\n\n");

    printf("\trace_checker [-q | --quick] test1 [test2 [test3 [...]]]\n");
    printf("\t    Runs the specified tests only.\n\n");
    printf("Quick mode: run only up to %d/%d instructions before/after\n",
           QUICK_OFFSET_BEFORE, QUICK_OFFSET_AFTER);
    printf("system call instruction (where most interesting races happen).\n");
}

void list_tests(void) {
    int i;

    printf("  %-20s Expected result\n", "Test name");
    for (i = 0; i < 2+20+1+strlen("Expected result"); i++)
        printf("-");
    printf("\n");
    for (i = 0; tests[i].name != NULL; i++) {
        printf("%c %-20s %s\n",
               tests[i].in_most ? ' ' : '*',
               tests[i].name,
               tests[i].expected == SUCCESS ? "success"
                                            : "failure");
    }
    printf("\n* - slow test - not included in the 'most tests' set\n");
}

int main(int argc, char **argv) {
    int i, run_all = 0, run_most = 0, run_specific = 0, unexpected = 0;

    setup_for_wait_for_sigchld();

    /*
     * Parse options manually so we can support long options without requiring
     * GNU getopt.
     */
    while (argc > 1) {
        --argc; ++argv;
        if (strncmp(*argv, "--", 2) == 0) {
            /* Long option */
            if (   strcmp(*argv, "--list-tests") == 0
                || strcmp(*argv, "--list") == 0) {
                list_tests();
                return 0;
            } else if (   strcmp(*argv, "--run-all-tests") == 0
                       || strcmp(*argv, "--all-tests") == 0
                       || strcmp(*argv, "--run-all") == 0
                       || strcmp(*argv, "--all") == 0) {
                run_all = 1;
            } else if (   strcmp(*argv, "--run-most-tests") == 0
                       || strcmp(*argv, "--most-tests") == 0
                       || strcmp(*argv, "-run-most") == 0
                       || strcmp(*argv, "--most") == 0) {
               run_most = 1;
            } else if (   strcmp(*argv, "--quick-mode") == 0
                       || strcmp(*argv, "--quick") == 0) {
                quick_mode = 1;
            } else {
                fprintf(stderr, "Unknown long option '%s'.\n\n", *argv);
                help();
                return 1;
            }
        } else if ((*argv)[0] == '-') {
            /* Short options */
            char *argchar;
            for (argchar = (*argv)+1; *argchar; ++argchar) {
                switch (*argchar) {
                    case 'l': list_tests(); return 0;
                    case 'a': run_all = 1; break;
                    case 'm': run_most = 1; break;
                    case 'h': help(); return 0;
                    case 'q': quick_mode = 1; break;
                    default:  fprintf(stderr, "Unknown short option '%c'.\n\n",
                                      *argchar);
                              help();
                              return 1;
                }
            }
        } else {
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
        }
    }

    if (!run_all && !run_most && !run_specific) {
        fprintf(stderr, "No tests given.\n\n");
        help();
        return 1;
    }

    if (run_all + run_most + run_specific > 1) {
        fprintf(stderr, "Conflicting options about which tests to run.\n\n");
        help();
        return 1;
    }

    /* Run all tests */
    for (i = 0; tests[i].name != NULL; i++) {
        if (tests[i].should_run || run_all || (run_most && tests[i].in_most)) {
            tests[i].result = run_test(&tests[i]);
        }
    }

    printf("\n\n\n\n\n");
    printf("  %-20s %-20s %-20s\n", "Test", "Result", "Expected");
    for (i = 0; i < 2 + 20 + 1 + 20 + 1 + 20; ++i) {
        printf("-");
    }
    printf("\n");
    for (i = 0; tests[i].name != NULL; i++) {
        if (tests[i].result != NOT_RUN) {
            if (tests[i].result != tests[i].expected) {
                unexpected ++;
            }
            printf("%c %-20s %-20s %-20s\n",
                    tests[i].result != tests[i].expected ? '*' : ' ',
                    tests[i].name,
                    labels[tests[i].result],
                    labels[tests[i].expected]);
        }
    }
    if (unexpected) {
        printf("\n* - %s %d test%s did not return the expected result.\n",
               unexpected == 1 ? "This" : "These",
               unexpected,
               unexpected == 1 ? "" : "s");
        return 1;
    }

    return 0;
}
