/** @file
 * Platform-independent portion of the <tt>sigsafe</tt> implementation.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#define ORG_SLAMB_SIGSAFE_INTERNAL
#include <sigsafe.h>
#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>

pthread_key_t sigsafe_key;
static pthread_once_t sigsafe_once = PTHREAD_ONCE_INIT;
sigsafe_user_handler_t user_handlers[SIGSAFE_NSIG];

#define SYSCALL(name, args) \
        extern void *_sigsafe_##name##_minjmp; \
        extern void *_sigsafe_##name##_maxjmp; \
        extern void *_sigsafe_##name##_jmpto;
#include "syscalls.h"
#undef SYSCALL

#define SYSCALL(name, args) \
        { #name, &sigsafe_##name, &_sigsafe_##name##_minjmp, \
          &_sigsafe_##name##_maxjmp, &_sigsafe_##name##_jmpto },
struct sigsafe_syscall sigsafe_syscalls[] = {
#include "syscalls.h"
    { NULL, NULL, NULL, NULL, NULL }
};
#undef SYSCALL

static void sighandler(int signum, siginfo_t *siginfo, ucontext_t *ctx) {
    struct sigsafe_tsd *tsd = pthread_getspecific(sigsafe_key);
    assert(0 <= signum && signum < SIGSAFE_NSIG);
    if (tsd != NULL) {
        if (user_handlers[signum] != NULL) {
            user_handlers[signum](signum, siginfo, ctx, tsd->user_data);
        }
        tsd->signal_received = 1;
        sighandler_for_platform(ctx);
    }
}

static void tsd_destructor(void* tsd_v) {
    struct sigsafe_tsd *tsd = (struct sigsafe_tsd*) tsd_v;
    if (tsd->destructor != NULL) {
        tsd->destructor(tsd->user_data);
    }
    free(tsd);
}

static void sigsafe_init(void) {
    /* "volatile" so our seemingly-useless references aren't optimized away. */
    volatile void *fp;

    pthread_key_create(&sigsafe_key, &tsd_destructor);

    /*
     * XXX
     * bbraun and landorf on #opendarwin tell me that dyld commonly deadlocks
     * in the signal handlers; it is not reentrant. Workaround: ensure all
     * symbols we reference in our signal handler are evaluated beforehand.
     * Should test this and note it in the documentation, so the userhandler
     * does the same thing.
     */
    fp = &pthread_getspecific;
    fp = &sighandler_for_platform;
    fp = &abort;
}

int sigsafe_install_handler(int signum, sigsafe_user_handler_t handler) {
    struct sigaction sa;

    assert(0 <= signum && signum < SIGSAFE_NSIG);
    pthread_once(&sigsafe_once, &sigsafe_init);
    user_handlers[signum] = handler;
    sa.sa_sigaction = (void*) &sighandler;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;

    if (sigaction(signum, &sa, NULL) != 0) {
        return -errno;
    }
    return 0;
}

int sigsafe_install_tsd(intptr_t user_data, void (*destructor)(intptr_t)) {
    struct sigsafe_tsd *tsd = NULL;
    int retval;

    assert(pthread_getspecific(sigsafe_key) == NULL);

    tsd = (struct sigsafe_tsd*) malloc(sizeof(struct sigsafe_tsd));
    if (tsd == NULL) {
        return -ENOMEM;
    }

    tsd->signal_received = 0;
    tsd->user_data = user_data;
    tsd->destructor = destructor;

    retval = pthread_setspecific(sigsafe_key, tsd);
    if (retval != 0) {
        free(tsd);
        return -retval;
    }

    return 0;
}

intptr_t sigsafe_clear_received(void) {
    struct sigsafe_tsd *tsd;

    tsd = (struct sigsafe_tsd*) pthread_getspecific(sigsafe_key);
    assert(tsd != NULL);
    tsd->signal_received = 0;
    return tsd->user_data;
}
