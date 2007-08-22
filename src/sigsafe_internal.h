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

#if defined(__APPLE_CC__)
#define HIDDEN_DEC __private_extern__
#define HIDDEN_DEF __private_extern__
#define INTERNAL_DEC __private_extern__
#define INTERNAL_DEF __private_extern__
#elif defined(__GNUC__)
#define HIDDEN_DEC   extern __attribute__ ((visibility ("hidden")))
#define HIDDEN_DEF          __attribute__ ((visibility ("hidden")))
#define INTERNAL_DEC extern __attribute__ ((visibility ("internal")))
#define INTERNAL_DEF        __attribute__ ((visibility ("internal")))
#else
#warn "Don't know how to make symbols private on your platform."
#define HIDDEN_DEC
#define HIDDEN_DEF
#define INTERNAL_DEC
#define INTERNAL_DEF
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
    void* const minjmp;
    void* const maxjmp;
    void* const jmpto;
};

INTERNAL_DEC struct sigsafe_syscall_ sigsafe_syscalls_[];

#ifdef SIGSAFE_NO_SIGINFO
HIDDEN_DEC void sigsafe_handler_for_platform_(struct sigcontext *ctx);
#else
HIDDEN_DEC void sigsafe_handler_for_platform_(ucontext_t *ctx);
#endif

#endif /* !SIGSAFE_INTERNAL_H */
