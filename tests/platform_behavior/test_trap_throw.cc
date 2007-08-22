/**
 * @file
 * Tests if trapping signal handlers can throw C++ exceptions.
 * This is very system-dependent and probably also dependent on the compiler
 * options. On gcc, try <tt>-fnon-call-exceptions</tt> and maybe
 * <tt>-fasynchronous-exceptions</tt>.
 * <p>This actually isn't working for me at all - it's calling the unexpected
 * handler. <tt>-fno-enforce-eh-specs</tt> doesn't help.</p>
 * @legal
 * Copyright &copy; 2004 &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#include <signal.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <exception>

struct received_signal { int signum; received_signal(int s) : signum(s) {} };

int
error_wrap(int retval, const char *funcname)
{
    if (retval < 0) {
        fprintf(stderr, "%s returned %d (errno==%d) (%s)\n",
                funcname, retval, errno, strerror(errno));
        abort();
    }
    return retval;
}

void
throwsigaction(int signum, siginfo_t *info, void *ctx)
{
    /*
     * We can get away with a printf() because we know exactly when this code
     * is invoked. Synchronous signals are easier.
     */
    printf("Signal handler on signal %d (code == %d)\n",
           signum, info->si_code);
    /*if (signum == SIGSEGV && info->si_code == SEGV_MAPERR) {*/
        printf("Throwing exception\n");
        throw received_signal(signum);
    /*} else {
        printf("Unexpected signum/si_code\n");
        abort();
    }*/
}

int
main(int, char**)
{
    struct sigaction sa;
    char c, *cp = NULL;

#if __GNUC__ >= 3
    std::set_terminate(__gnu_cxx::__verbose_terminate_handler);
#endif

    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = &throwsigaction;
    error_wrap(sigemptyset(&sa.sa_mask), "sigemptyset");
    error_wrap(sigaction(SIGSEGV, &sa, NULL), "sigaction");
    error_wrap(sigaction(SIGBUS , &sa, NULL), "sigaction");
    try {
        c = (*cp);
    } catch (received_signal &s) {
        printf("Caught signal %d\n", s.signum);
    }
    return 0;
}
