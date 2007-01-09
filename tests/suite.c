#ifdef _THREAD_SAFE
#include <pthread.h>
#endif
#include <sigsafe.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

sig_atomic_t volatile tsd;

enum error_return_type {
    DIRECT,         /**< pthread functions */
    NEGATIVE,       /**< sigsafe functions */
    ERRNO           /**< old-school functions */
};

int
error_wrap(int retval, const char *funcname, enum error_return_type type)
{
    if (type == ERRNO && retval < 0) {
        fprintf(stderr, "%s returned %d (errno==%d) (%s)\n",
                funcname, retval, errno, strerror(errno));
    } else if (type == DIRECT && retval != 0) {
        fprintf(stderr, "%s returned %d (%s)\n",
                funcname, retval, strerror(retval));
    } else if (type == NEGATIVE && retval < 0) {
        fprintf(stderr, "%s returned %d (%s)\n",
                funcname, retval, strerror(-retval));
    }
    return retval;
}

/*
 * These macros are used by various syscalls as a sanity check that the
 * system function call convention is returned; all callee-preserved
 * registers are indeed preserved.
 */
#define REGISTERS_DECLARATION   register int a, b, c, d, e, f
#define REGISTERS_PRE           do { a = 0x11111111; \
                                     b = 0x22222222; \
                                     c = 0x33333333; \
                                     d = 0x44444444; \
                                     e = 0x55555555; \
                                     f = 0x66666666; } while (0)
#define REGISTERS_WRONG         (a != 0x11111111 || \
                                 b != 0x22222222 || \
                                 c != 0x33333333 || \
                                 d != 0x44444444 || \
                                 e != 0x55555555 || \
                                 f != 0x66666666)

/**
 * Ensures a signal delivered well before the syscall causes EINTR,
 * and that it can be properly cleared.
 *
 * This doubles as a basic test of sigsafe_nanosleep(), which is remarkable
 * under OS X because it is a Mach system call, different from the others.
 */
int
test_received_flag(void)
{
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000 };
    int res;

    res = sigsafe_nanosleep(&ts, NULL);
    if (res != 0) {
        return 1;
    }
    raise(SIGALRM);
    res = sigsafe_nanosleep(&ts, NULL);
    if (res != -EINTR) {
        return 1;
    }
    res = sigsafe_nanosleep(&ts, NULL);
    if (res != -EINTR) {
        return 1;
    }
    sigsafe_clear_received();
    res = sigsafe_nanosleep(&ts, NULL);
    if (res != 0) {
        return 1;
    }

    return 0;
}

/**
 * Tests that sigsafe_pause() works. This is a simple zero-argument system
 * call, except on platforms where it's implemented by calling sigsuspend().
 */
int
test_pause(void)
{
    struct itimerval it = {
        .it_interval = { .tv_sec = 0, .tv_usec = 0 },
        .it_value = { .tv_sec = 0, .tv_usec = 500 }
    };
    int res;
    REGISTERS_DECLARATION;

    error_wrap(setitimer(ITIMER_REAL, &it, NULL),
               "setitimer", ERRNO);
    REGISTERS_PRE;
    res = sigsafe_pause();
    if (REGISTERS_WRONG) {
        sigsafe_clear_received();
        return 1;
    }
    sigsafe_clear_received();
    if (res != -EINTR) {
        return 1;
    }

    return 0;
}

void
test_userhandler_handler(int signo, siginfo_t *si, ucontext_t *ctx,
                         intptr_t user_data)
{
    sig_atomic_t volatile *tsd = (sig_atomic_t volatile *) user_data;

    if (*tsd != 26) {
        abort();
    }
    *tsd = 37;
}

/**
 * Tests that the userhandler is invoked with the correct info.
 */
int
test_userhandler(void)
{
    sigsafe_install_handler(SIGUSR1, test_userhandler_handler);
    tsd = 26;
    raise(SIGUSR1);
    sigsafe_clear_received();
    if (tsd != 37) {
        return 1;
    }

    return 0;
}


#ifdef _THREAD_SAFE
#define MAGIC_INIT          73
#define MAGIC_BEFORESIG     26
#define MAGIC_SIG           37
#define MAGIC_AFTERSIG      17
#define MAGIC_DESTRUCTOR    42

static void
test_tsd_usr1(int signo, siginfo_t *si, ucontext_t *ctx, intptr_t user_data)
{
    sig_atomic_t volatile *subthread_tsd = (sig_atomic_t volatile *) user_data;

    write(1, "[userhandler]", sizeof("[userhandler]")-1);
    if (*subthread_tsd != MAGIC_BEFORESIG) {
        abort();
    }
    *subthread_tsd = MAGIC_SIG;
}

static void
subthread_tsd_destructor(intptr_t tsd)
{
    sig_atomic_t volatile *subthread_tsd = (sig_atomic_t volatile *) tsd;

    if (*subthread_tsd != MAGIC_AFTERSIG) {
        abort();
    }
    *subthread_tsd = MAGIC_DESTRUCTOR;
    printf("[destructed %p]", subthread_tsd);
    fflush(stdout);
}

static void*
test_tsd_subthread(void *arg)
{
    sig_atomic_t volatile *subthread_tsd = (sig_atomic_t volatile *) arg;

    printf("[pre-install %p]", subthread_tsd);
    fflush(stdout);
    error_wrap(sigsafe_install_tsd((intptr_t) subthread_tsd,
                                   subthread_tsd_destructor),
               "sigsafe_install_tsd", NEGATIVE);

    if (*subthread_tsd != MAGIC_INIT) {
        return (void*) 1;
    }
    *subthread_tsd = MAGIC_BEFORESIG;
    error_wrap(sigsafe_install_handler(SIGUSR1, test_tsd_usr1),
               "sigsafe_install_handler", NEGATIVE);
    write(1, "[pre-kill]", sizeof("[pre-kill]")-1);
    pthread_kill(pthread_self(), SIGUSR1);
    /*
     * Note: never clearing received.
     * This should not affect the main thread.
     */
    if (*subthread_tsd != MAGIC_SIG) {
        return (void*) 1;
    }
    *subthread_tsd = MAGIC_AFTERSIG;

    write(1, "[returning]", sizeof("[returning]")-1);
    return (void*) 0;
}

int
test_tsd(void)
{
    pthread_t subthread;
    sig_atomic_t volatile subthread_tsd = MAGIC_INIT;
    void *vres;
    int ires;
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 1 };

    tsd = 0;
    write(1, "[pre-create]", sizeof("[pre-create]")-1);

    error_wrap(pthread_create(&subthread, NULL, test_tsd_subthread,
                              (void*) &subthread_tsd),
               "pthread_create", DIRECT);
    write(1, "[pre-join]", sizeof("[pre-join]")-1);
    error_wrap(pthread_join(subthread, &vres),
               "pthread_join", DIRECT);
    write(1, "[post-join]", sizeof("[post-join]")-1);
    if (vres != 0) {
        printf("(subthread failed) ");
        return 1;
    }
    if (subthread_tsd != MAGIC_DESTRUCTOR) {
        printf("(destructor didn't run; subthread_tsd=*%p=%d tsd=*%p=%d) ",
               &subthread_tsd, subthread_tsd, &tsd, tsd);
        return 1;
    }

    /* Subthread's flag shouldn't be honored here. */
    write(1, "[pre-nanosleep]", sizeof("[pre-nanosleep]")-1);
    ires = sigsafe_nanosleep(&ts, NULL);
    if (ires != 0) {
        printf("(sigsafe_nanosleep failed) ");
        return 1;
    }

    return 0;
}
#undef MAGIC_INIT
#undef MAGIC_BEFORESIG
#undef MAGIC_SIG
#undef MAGIC_AFTERSIG
#undef MAGIC_DESTRUCTOR
#endif

/* Tests that sigsafe_read() works. */
int
test_read(void)
{
    int mypipe[2];
    int res;
    REGISTERS_DECLARATION;
    char buf[4];

    error_wrap(pipe(mypipe), "pipe", ERRNO);
    res = write(mypipe[1], "asdf", 4); /* <PIPE_BUF; complete without block */
    if (res != 4) {
        printf("(setup failure) ");
        res = 1;
        goto out;
    }

    REGISTERS_PRE;
    res = sigsafe_read(mypipe[0], buf, 4);
    if (REGISTERS_WRONG) {
        printf("(bad registers) ");
        res = 1;
        goto out;
    }
    if (res != 4) {
        printf("(returned %d) ", res);
        res = 1;
        goto out;
    }
    if (memcmp(buf, "asdf", 4) != 0) {
        res = 1;
        goto out;
    }
    res = 0;

  out:
    close(mypipe[0]);
    close(mypipe[1]);
    return res;
}

struct test {
    char *name;
    int (*func)(void);
} tests[] = {
#define DECLARE(name) { #name, name }
    DECLARE(test_received_flag),
    DECLARE(test_pause), /* 0-argument */
    DECLARE(test_read),  /* 3-argument */
    DECLARE(test_userhandler),
#ifdef _THREAD_SAFE
    DECLARE(test_tsd),
#endif
#undef DECLARE
};

void
sighup(int signo)
{
    abort();
}

int
main(int argc, char **argv)
{
    int result = 0;
    struct test *t;

    /* the i386-linux tests are dying on signal 1?!? */
    signal(1 /*SIGHUP*/, sighup);

    error_wrap(sigsafe_install_handler(SIGALRM, NULL),
               "sigsafe_install_handler", NEGATIVE);

    error_wrap(sigsafe_install_tsd((intptr_t) &tsd, NULL),
               "sigsafe_install_tsd", NEGATIVE);

    for (t = tests; t < tests + sizeof(tests)/sizeof(tests[0]); t++) {
        int this_result;

        printf("%s: ", t->name);
        fflush(stdout);
        this_result = t->func();
        printf("%s\n", (this_result == 0) ? "success" : "FAILURE");
        result |= this_result;
    }

    return (result == 0) ? 0 : 1;
}
