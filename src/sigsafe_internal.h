/** @file
 * Internal common definitions for sigsafe.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#include "sigsafe.h"

#ifndef SIGSAFE_INTERNAL_H
#define SIGSAFE_INTERNAL_H

/*
 * See Ulrich Drepper's "How to Write Shared Libraries"
 * <http://people.redhat.com/drepper/dsohowto.pdf>
 * for a description of __attribute__ ((visibility ("hidden")))
 * and other good things to know about ELF relocation, symbol versioning, etc.
 */

#if defined(__GNUC__) && defined(PIC)
#define PRIVATE __attribute__ ((visibility ("hidden")))
#else
#define PRIVATE
#endif

/**
 * @define SIGMAX
 * The highest used signal number.
 * Note that the <tt>NSIG</tt> many platforms has is misnamed - it's not the
 * number of signals, but the highest number + 1.
 */
#if defined(SIGMAX)
#define SIGSAFE_SIGMAX SIGMAX
#elif defined(NSIG)
#define SIGSAFE_SIGMAX (NSIG-1)
#elif defined(_NSIG)
#define SIGSAFE_SIGMAX (_NSIG-1)
#else
#error Not sure how many signals you have
#endif

/** Thread-specific data. */
struct sigsafe_tsd_ {
    /** Non-zero iff signal received since last sigsafe_clear_received. */
    volatile sig_atomic_t signal_received;
    intptr_t user_data;
    void (*destructor)(intptr_t);
};

struct sigsafe_syscall_ {
    const void *minjmp;
    const void *maxjmp;
    const void *jmpto;
};

extern struct sigsafe_syscall_ sigsafe_syscalls_[] PRIVATE;

#ifdef SIGSAFE_NO_SIGINFO
void sigsafe_handler_for_platform_(struct sigcontext *ctx) PRIVATE;
#else
void sigsafe_handler_for_platform_(ucontext_t *ctx) PRIVATE;
#endif

#endif /* !SIGSAFE_INTERNAL_H */
