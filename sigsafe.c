/**
 * Platform-independent portion of the <tt>sigsafe</tt> implementation.
 * @version $Id$
 * @author  Scott Lamb &lt;slamb@slamb.org&gt;
 */

#define ORG_SLAMB_SIGSAFE_INTERNAL
#include <sigsafe.h>
#include <pthread.h>
#include <assert.h>
#include <stdlib.h>

pthread_key_t sigsafe_key = PTHREAD_KEY_INITIALIZER;
void (*user_handlers)(int, siginfo_t*, ucontext_t*, intptr_t)[_NSIGS];

static void sighandler(int signum, siginfo_t *siginfo, ucontext_t *ctx) {
    struct sigsafe_tsd *tsd = pthread_getspecific(sigsafe_key);
    assert(0 <= signum && signum < _NSIGS);
    if (tsd != NULL) {
        if (user_handlers[signum] != NULL) {
            user_handlers[signum](signum, siginfo, ctx, tsd->user_data);
        }
        tsd->signal_received = 1;
        sighandler_for_platform(ctx);
    }
}

int sigsafe_install_handler(int signum,
        void (*handler)(int, siginfo_t*, ucontext_t*, intptr_t)) {
    struct sigaction sa;

    assert(0 <= signum && signum < _NSIGS);
    user_handlers[signum] = handler;
    sa.sa_sigaction = &sighandler;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    return sigaction(signum, &sa, NULL);
}

static void tsd_destructor(void* tsd_v) {
    struct sigsafe_tsd *tsd = (struct sigsafe_tsd*) tsd_v;
    if (tsd->destructor != NULL) {
        tsd->destructor(user_data);
    }
    free(tsd);
}

int sigsafe_install_tsd(intptr_t user_data, void (*destructor)(intptr_t)) {
    struct sigsafe_tsd *tsd = NULL;
    int retval;

    assert(pthread_getspecific(sigsafe_key) == NULL);

    tsd = (struct sigsafe_tsd*) malloc(sizeof(struct sigsafe_tsd));
    if (tsd == NULL) {
        errno = ENOMEM;
        return -1;
    }

    tsd->signal_received = 0;
    tsd->user_data = user_data;
    tsd->destructor = destructor;

    retval = pthread_setspecific(sigsafe_key, tsd)
    if (retval != 0) {
        free(tsd);
        errno = retval;
        return -1;
    }

    return 0;
}
