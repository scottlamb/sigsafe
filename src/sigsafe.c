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

#ifdef _THREAD_SAFE
pthread_key_t sigsafe_key;
static pthread_once_t sigsafe_once = PTHREAD_ONCE_INIT;
#else
struct sigsafe_tsd *sigsafe_data;
static int sigsafe_inited;
#endif

sigsafe_user_handler_t user_handlers[SIGSAFE_SIGMAX];

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
#ifdef _THREAD_SAFE
    struct sigsafe_tsd *sigsafe_data = pthread_getspecific(sigsafe_key);
#endif
    assert(0 < signum && signum <= SIGSAFE_SIGMAX);
#ifdef ORG_SLAMB_SIGSAFE_DEBUG_SIGNAL
    write(2, "[S]", 3);
#endif
    if (sigsafe_data != NULL) {
        if (user_handlers[signum - 1] != NULL) {
            user_handlers[signum - 1](signum, siginfo, ctx, sigsafe_data->user_data);
        }
        sigsafe_data->signal_received = 1;
        sighandler_for_platform(ctx);
    }
}

#ifdef _THREAD_SAFE
static void tsd_destructor(void* tsd_v) {
    struct sigsafe_tsd *sigsafe_data = (struct sigsafe_tsd*) tsd_v;
#else
static void tsd_destructor(void) {
#endif
    if (sigsafe_data->destructor != NULL) {
        sigsafe_data->destructor(sigsafe_data->user_data);
    }
    free(sigsafe_data);
}

static void sigsafe_init(void) {
    /* "volatile" so our seemingly-useless references aren't optimized away. */
    volatile void *fp;

#ifdef _THREAD_SAFE
    pthread_key_create(&sigsafe_key, &tsd_destructor);
#else
    atexit(&tsd_destructor);
#endif

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
    fp = &write;
}

int sigsafe_install_handler(int signum, sigsafe_user_handler_t handler) {
    struct sigaction sa;

    assert(0 < signum && signum <= SIGSAFE_SIGMAX);
#ifdef _THREAD_SAFE
    pthread_once(&sigsafe_once, &sigsafe_init);
#else
    if (!sigsafe_inited) {
        sigsafe_inited = 1;
        sigsafe_init();
    }
#endif
    user_handlers[signum - 1] = handler;
    sa.sa_sigaction = (void*) &sighandler;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;

    /*
     * Mask all signals to ensure a sigsafe handler never interrupts another.
     */
    sigfillset(&sa.sa_mask);

    if (sigaction(signum, &sa, NULL) != 0) {
        return -errno;
    }
    return 0;
}

int sigsafe_install_tsd(intptr_t user_data, void (*destructor)(intptr_t)) {
#ifdef _THREAD_SAFE
    struct sigsafe_tsd *sigsafe_data = NULL;
    int retval;

    assert(pthread_getspecific(sigsafe_key) == NULL);
#else
    assert(sigsafe_data == NULL);
#endif

    sigsafe_data = (struct sigsafe_tsd*) malloc(sizeof(struct sigsafe_tsd));
    if (sigsafe_data == NULL) {
        return -ENOMEM;
    }

    sigsafe_data->signal_received = 0;
    sigsafe_data->user_data = user_data;
    sigsafe_data->destructor = destructor;

#ifdef _THREAD_SAFE
    retval = pthread_setspecific(sigsafe_key, sigsafe_data);
    if (retval != 0) {
        free(sigsafe_data);
        return -retval;
    }
#endif

    return 0;
}

intptr_t sigsafe_clear_received(void) {
#ifdef _THREAD_SAFE
    struct sigsafe_tsd *sigsafe_data;

    sigsafe_data = (struct sigsafe_tsd*) pthread_getspecific(sigsafe_key);
#endif
    assert(sigsafe_data != NULL);
    sigsafe_data->signal_received = 0;
    return sigsafe_data->user_data;
}
