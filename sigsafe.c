/**
 * Platform-independent portion of the <tt>sigsafe</tt> implementation.
 * @version $Id$
 * @author  Scott Lamb &lt;slamb@slamb.org&gt;
 */

#include <sigsafe.h>
#include <pthread.h>

struct sigsafe_tsd {
    volatile sig_atomic_t signal_received;
    sigset_t active_mask;
    intptr_t user_data;
    void (*destructor)(intptr_t);
};

static pthread_key_t sigsafe_key = PTHREAD_KEY_INITIALIZER;
static void (*user_handlers)(int, siginfo_t*, ucontext_t*, intptr_t)[_NSIGS];

void sighandler_for_platform(ucontext_t *ctx);

static void sighandler(int signum, siginfo_t *siginfo, ucontext_t *ctx) {
    struct sigsafe_tsd *tsd = pthread_getspecific(sigsafe_key);
    assert(0 <= signum && signum < _NSIGS);
    if (tsd != NULL) {
        user_handlers[signum](signum, siginfo, ctx, tsd->user_data);
    }
    tsd->signal_received = 1;
    sighandler_for_platform(ctx);
}

int sigsafe_install_handler(int signum,
        void (*handler)(int, siginfo_t*, ucontext_t*, intptr_t)) {
    struct sigaction sa;

    assert(0 <= signum && signum < _NSIGS);
    user_handlers[signum] = handler;
    sa.sa_sigaction = &sighandler;
    sigfillset(&sa.sa_mask);
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

    assert(pthread_getspecific(sigsafe_key) == NULL);

    tsd = (struct sigsafe_tsd*) malloc(sizeof(struct sigsafe_tsd));
    if (tsd == NULL) {
        errno = ENOMEM;
        return -1;
    }

    tsd->signal_received = 0;
    sigemptyset(&tsd->active_mask); /** @todo Fill in active_mask */
    tsd->user_data = user_data;
    tsd->destructor = destructor;

    if (pthread_setspecific(sigsafe_key, tsd) == -1) {
        free(tsd);
        return -1;
    }

    return 0;
}

int sigsafe_setmask(int how, const sigset_t *set, sigset_t *oset) {
    struct sigsafe_tsd *tsd = NULL;

    tsd = (struct sigsafe_tsd*) pthread_getspecific(sigsafe_key);
    assert(tsd != NULL);

    /** @todo */
}
