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
#include <errno.h>

pthread_key_t sigsafe_key;
static pthread_once_t sigsafe_once = PTHREAD_ONCE_INIT;
sigsafe_user_handler_t user_handlers[SIGSAFE_NSIG];

extern void *_sigsafe_read_minjmp;
extern void *_sigsafe_read_maxjmp;
extern void *_sigsafe_read_jmpto;
#if 0
extern void *_sigsafe_write_minjmp;
extern void *_sigsafe_write_maxjmp;
extern void *_sigsafe_write_jmpto;
extern void *_sigsafe_readv_minjmp;
extern void *_sigsafe_readv_maxjmp;
extern void *_sigsafe_readv_jmpto;
extern void *_sigsafe_writev_minjmp;
extern void *_sigsafe_writev_maxjmp;
extern void *_sigsafe_writev_jmpto;
#ifdef HAVE_EPOLL_WAIT
extern void *_sigsafe_epoll_wait_minjmp;
extern void *_sigsafe_epoll_wait_maxjmp;
extern void *_sigsafe_epoll_wait_jmpto;
#endif
extern void *_sigsafe_kevent_minjmp;
extern void *_sigsafe_kevent_maxjmp;
extern void *_sigsafe_kevent_jmpto;
extern void *_sigsafe_select_minjmp;
extern void *_sigsafe_select_maxjmp;
extern void *_sigsafe_select_jmpto;
#ifdef HAVE_POLL
extern void *_sigsafe_poll_minjmp;
extern void *_sigsafe_poll_maxjmp;
extern void *_sigsafe_poll_jmpto;
#endif
extern void *_sigsafe_wait4_minjmp;
extern void *_sigsafe_wait4_maxjmp;
extern void *_sigsafe_wait4_jmpto;
extern void *_sigsafe_accept_minjmp;
extern void *_sigsafe_accept_maxjmp;
extern void *_sigsafe_accept_jmpto;
extern void *_sigsafe_connect_minjmp;
extern void *_sigsafe_connect_maxjmp;
extern void *_sigsafe_connect_jmpto;
#ifdef HAVE_NANOSLEEP
/*
 * On Darwin, this is implemented in terms of some Mach thing instead of a
 * normal system call.
 */
extern void *_sigsafe_nanosleep_minjmp;
extern void *_sigsafe_nanosleep_maxjmp;
extern void *_sigsafe_nanosleep_jmpto;
#endif
#endif

struct sigsafe_syscall sigsafe_syscalls[] = {
    { "read",       &sigsafe_read,       &_sigsafe_read_minjmp,       &_sigsafe_read_maxjmp,       &_sigsafe_read_jmpto       },
#if 0
    { "readv",      &sigsafe_readv,      &_sigsafe_readv_minjmp,      &_sigsafe_read_maxjmp,       &_sigsafe_read_jmpto       },
    { "write",      &sigsafe_write,      &_sigsafe_write_minjmp,      &_sigsafe_write_maxjmp,      &_sigsafe_write_jmpto      },
    { "writev",     &sigsafe_writev,     &_sigsafe_writev_minjmp,     &_sigsafe_write_maxjmp,      &_sigsafe_write_jmpto      },
#ifdef HAVE_EPOLL_WAIT
    { "epoll_wait", &sigsafe_epoll_wait, &_sigsafe_epoll_wait_minjmp, &_sigsafe_epoll_wait_maxjmp, &_sigsafe_epoll_wait_jmpto },
#endif
    { "kevent",     &sigsafe_kevent,     &_sigsafe_kevent_minjmp,     &_sigsafe_kevent_maxjmp,     &_sigsafe_kevent_maxjmp    },
    { "select",     &sigsafe_select,     &_sigsafe_select_minjmp,     &_sigsafe_select_maxjmp,     &_sigsafe_select_maxjmp    },
#ifdef HAVE_POLL
    { "poll",       &sigsafe_poll,       &_sigsafe_poll_minjmp,       &_sigsafe_poll_maxjmp,       &_sigsafe_poll_jmpto       },
#endif
    { "wait4",      &sigsafe_wait4,      &_sigsafe_wait4_minjmp,      &_sigsafe_wait4_maxjmp,      &_sigsafe_wait4_jmpto      },
    { "accept",     &sigsafe_accept,     &_sigsafe_accept_minjmp,     &_sigsafe_accept_maxjmp,     &_sigsafe_accept_jmpto     },
    { "connect",    &sigsafe_connect,    &_sigsafe_connect_minjmp,    &_sigsafe_connect_maxjmp,    &_sigsafe_connect_jmpto    },
#ifdef HAVE_NANOSLEEP
    { "nanosleep",  &sigsafe_nanosleep,  &_sigsafe_nanosleep_minjmp,  &_sigsafe_nanosleep_maxjmp,  &_sigsafe_nanosleep_jmpto  },
#endif
#endif
    { NULL,         NULL,                NULL,                       NULL,                       NULL                      }
};

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
    pthread_key_create(&sigsafe_key, &tsd_destructor);
}

int sigsafe_install_handler(int signum,
        void (*handler)(int, siginfo_t*, ucontext_t*, intptr_t)) {
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
    int retval;

    tsd = (struct sigsafe_tsd*) pthread_getspecific(sigsafe_key);
    assert(tsd != NULL);
    tsd->signal_received = 0;
    return tsd->user_data;
}
