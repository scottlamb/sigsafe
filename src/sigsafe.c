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

pthread_key_t sigsafe_key;
static pthread_once_t sigsafe_once = PTHREAD_ONCE_INIT;
sigsafe_user_handler_t user_handlers[NSIG];

extern void *sigsafe_read_minjmp;
extern void *sigsafe_read_maxjmp;
extern void *sigsafe_read_jmpto;
extern void *sigsafe_write_minjmp;
extern void *sigsafe_write_maxjmp;
extern void *sigsafe_write_jmpto;
extern void *sigsafe_readv_minjmp;
extern void *sigsafe_readv_maxjmp;
extern void *sigsafe_readv_jmpto;
extern void *sigsafe_writev_minjmp;
extern void *sigsafe_writev_maxjmp;
extern void *sigsafe_writev_jmpto;
#ifdef HAVE_EPOLL_WAIT
extern void *sigsafe_epoll_wait_minjmp;
extern void *sigsafe_epoll_wait_maxjmp;
extern void *sigsafe_epoll_wait_jmpto;
#endif
extern void *sigsafe_kevent_minjmp;
extern void *sigsafe_kevent_maxjmp;
extern void *sigsafe_kevent_jmpto;
extern void *sigsafe_select_minjmp;
extern void *sigsafe_select_maxjmp;
extern void *sigsafe_select_jmpto;
#ifdef HAVE_POLL
extern void *sigsafe_poll_minjmp;
extern void *sigsafe_poll_maxjmp;
extern void *sigsafe_poll_jmpto;
#endif
extern void *sigsafe_wait4_minjmp;
extern void *sigsafe_wait4_maxjmp;
extern void *sigsafe_wait4_jmpto;
extern void *sigsafe_accept_minjmp;
extern void *sigsafe_accept_maxjmp;
extern void *sigsafe_accept_jmpto;
extern void *sigsafe_connect_minjmp;
extern void *sigsafe_connect_maxjmp;
extern void *sigsafe_connect_jmpto;

struct sigsafe_syscall sigsafe_syscalls[] = {
    { "read",       &sigsafe_read,       &sigsafe_read_minjmp,       &sigsafe_read_maxjmp,       &sigsafe_read_jmpto       },
    { "readv",      &sigsafe_readv,      &sigsafe_readv_minjmp,      &sigsafe_read_maxjmp,       &sigsafe_read_jmpto       },
    { "write",      &sigsafe_write,      &sigsafe_write_minjmp,      &sigsafe_write_maxjmp,      &sigsafe_write_jmpto      },
    { "writev",     &sigsafe_writev,     &sigsafe_writev_minjmp,     &sigsafe_write_maxjmp,      &sigsafe_write_jmpto      },
#ifdef HAVE_EPOLL_WAIT
    { "epoll_wait", &sigsafe_epoll_wait, &sigsafe_epoll_wait_minjmp, &sigsafe_epoll_wait_maxjmp, &sigsafe_epoll_wait_jmpto },
#endif
    { "kevent",     &sigsafe_kevent,     &sigsafe_kevent_minjmp,     &sigsafe_kevent_maxjmp,     &sigsafe_kevent_maxjmp    },
    { "select",     &sigsafe_select,     &sigsafe_select_minjmp,     &sigsafe_select_maxjmp,     &sigsafe_select_maxjmp    },
#ifdef HAVE_POLL
    { "poll",       &sigsafe_poll,       &sigsafe_poll_minjmp,       &sigsafe_poll_maxjmp,       &sigsafe_poll_jmpto       },
#endif
    { "wait4",      &sigsafe_wait4,      &sigsafe_wait4_minjmp,      &sigsafe_wait4_maxjmp,      &sigsafe_wait4_jmpto      },
    { "accept",     &sigsafe_accept,     &sigsafe_accept_minjmp,     &sigsafe_accept_maxjmp,     &sigsafe_accept_jmpto     },
    { "connect",    &sigsafe_connect,    &sigsafe_connect_minjmp,    &sigsafe_connect_maxjmp,    &sigsafe_connect_jmpto    },
    { NULL,         NULL,                NULL,                       NULL,                       NULL                      }
};

static void sighandler(int signum, siginfo_t *siginfo, ucontext_t *ctx) {
    struct sigsafe_tsd *tsd = pthread_getspecific(sigsafe_key);
    assert(0 <= signum && signum < NSIG);
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

    assert(0 <= signum && signum < NSIG);
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
