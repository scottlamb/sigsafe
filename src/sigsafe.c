/** @file
 * Platform-independent portion of the <tt>sigsafe</tt> implementation.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#include "sigsafe_internal.h"
#ifdef _THREAD_SAFE
#include <pthread.h>
#endif
#include <assert.h>
#include <stdlib.h>
#include <errno.h>

#ifdef _THREAD_SAFE
PRIVATE_DEC(pthread_key_t sigsafe_key_) = 0;
static pthread_once_t sigsafe_once = PTHREAD_ONCE_INIT;
#else
PRIVATE_DEC(struct sigsafe_tsd_* sigsafe_data_) = 0;
static int sigsafe_inited;
#endif

static sigsafe_user_handler_t user_handlers[SIGSAFE_SIGMAX];

#define SYSCALL(name, args) \
        PRIVATE_DEF(void sigsafe_##name##_minjmp_(void)); \
        PRIVATE_DEF(void sigsafe_##name##_maxjmp_(void)); \
        PRIVATE_DEF(void sigsafe_##name##_jmpto_ (void));
#define MACH_SYSCALL(name, args) SYSCALL(name, args)
#include "syscalls.h"
#undef SYSCALL

#define SYSCALL(name, args) \
        { sigsafe_##name##_minjmp_, \
          sigsafe_##name##_maxjmp_, \
          sigsafe_##name##_jmpto_ },
PRIVATE_DEC(struct sigsafe_syscall_ sigsafe_syscalls_[]) = {
#include "syscalls.h"
    { NULL, NULL, NULL }
};
#undef SYSCALL
#undef MACH_SYSCALL

#ifdef SIGSAFE_NO_SIGINFO
static void sighandler(int signum, int code, struct sigcontext *ctx) {
#else
static void sighandler(int signum, siginfo_t *siginfo, ucontext_t *ctx) {
#endif
#ifdef _THREAD_SAFE
    struct sigsafe_tsd_ *sigsafe_data_ = pthread_getspecific(sigsafe_key_);
#endif
    assert(0 < signum && signum <= SIGSAFE_SIGMAX);
#ifdef SIGSAFE_DEBUG_SIGNAL
    write(2, "[S]", 3);
#endif
    if (sigsafe_data_ != NULL) {
        if (user_handlers[signum - 1] != NULL) {
#ifdef SIGSAFE_NO_SIGINFO
            user_handlers[signum - 1](signum, code, ctx,
                                      sigsafe_data_->user_data);
#else
            user_handlers[signum - 1](signum, siginfo, ctx,
                                      sigsafe_data_->user_data);
#endif
        }
        sigsafe_data_->signal_received = 1;
        sigsafe_handler_for_platform_(ctx);
    }
}

#ifdef _THREAD_SAFE
static void tsd_destructor(void* tsd_v) {
    struct sigsafe_tsd_ *sigsafe_data_ = (struct sigsafe_tsd_*) tsd_v;
    write(1, "[start tsd_destructor]", sizeof("[start tsd_destructor]")-1);
    if (sigsafe_data_->destructor != NULL) {
        sigsafe_data_->destructor(sigsafe_data_->user_data);
    }
    free(sigsafe_data_);
    write(1, "[end tsd_destructor]", sizeof("[end tsd_destructor]")-1);
}
#endif

static void sigsafe_init(void) {
    /* "volatile" so our seemingly-useless references aren't optimized away. */
    void * volatile fp;

#ifdef _THREAD_SAFE
    pthread_key_create(&sigsafe_key_, &tsd_destructor);
#endif

    /*
     * XXX
     * bbraun and landorf on #opendarwin tell me that dyld commonly deadlocks
     * in the signal handlers; it is not reentrant. Workaround: ensure all
     * symbols we reference in our signal handler are evaluated beforehand.
     * Should test this and note it in the documentation, so the userhandler
     * does the same thing.
     */
#ifdef _THREAD_SAFE
    fp = &pthread_getspecific;
#endif
    fp = &sigsafe_handler_for_platform_;
    fp = &write;
}

static void sigsafe_ensure_init(void) {
#ifdef _THREAD_SAFE
    pthread_once(&sigsafe_once, &sigsafe_init);
#else
    if (!sigsafe_inited) {
        sigsafe_inited = 1;
        sigsafe_init();
    }
#endif
}

int sigsafe_install_handler(int signum, sigsafe_user_handler_t handler) {
    struct sigaction sa;

    assert(0 < signum && signum <= SIGSAFE_SIGMAX);
    sigsafe_ensure_init();
    user_handlers[signum - 1] = handler;
#ifdef SIGSAFE_NO_SIGINFO
    sa.sa_handler = (void (*)(int)) &sighandler;
    sa.sa_flags = SA_RESTART;
#else
    sa.sa_sigaction = (void*) &sighandler;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
#endif

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
    struct sigsafe_tsd_ *sigsafe_data_ = NULL;
    int retval;

    sigsafe_ensure_init();
    assert(pthread_getspecific(sigsafe_key_) == NULL);
#else
    assert(sigsafe_data_ == NULL);
#endif

    sigsafe_data_ = (struct sigsafe_tsd_*) malloc(sizeof(struct sigsafe_tsd_));
    if (sigsafe_data_ == NULL) {
        return -ENOMEM;
    }

    sigsafe_data_->signal_received = 0;
    sigsafe_data_->user_data = user_data;
    sigsafe_data_->destructor = destructor;

#ifdef _THREAD_SAFE
    retval = pthread_setspecific(sigsafe_key_, sigsafe_data_);
    if (retval != 0) {
        free(sigsafe_data_);
        return -retval;
    }
#endif

    return 0;
}

intptr_t sigsafe_clear_received(void) {
#ifdef _THREAD_SAFE
    struct sigsafe_tsd_ *sigsafe_data_;

    sigsafe_data_ = (struct sigsafe_tsd_*) pthread_getspecific(sigsafe_key_);
#endif
    assert(sigsafe_data_ != NULL);
    sigsafe_data_->signal_received = 0;
    return sigsafe_data_->user_data;
}
