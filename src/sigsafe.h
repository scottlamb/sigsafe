/** @file
 * Alternate system call wrappers which provide signal safety.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#ifndef ORG_SLAMB_SIGSAFE_H
#define ORG_SLAMB_SIGSAFE_H

#include <pthread.h>
#include <signal.h>
#include <ucontext.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>
#ifdef SIGSAFE_HAVE_STDINT_H
#include <stdint.h> /* for intptr_t */
#endif

#ifdef HAVE_EPOLL
/*
 * RedHat 9's epoll header needs stdint.h but doesn't include it itself.
 */
#include <stdint.h>
#include <sys/epoll.h>
#endif

#ifdef HAVE_POLL
#include <sys/poll.h>
#endif
#include <unistd.h>
#include <stddef.h>
#include <setjmp.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup sigsafe_control Signal control functions
 */

/**
 * User-specified handler type.
 * @see sigsafe_install_handler
 * @ingroup sigsafe_control
 */
typedef void (*sigsafe_user_handler_t)(int, siginfo_t*, ucontext_t*, intptr_t);

/**
 * Installs a safe signal handler.
 * This installs a safe signal handler. It is <i>global</i> to the process.
 * Note that <i>nothing</i> will happen on signal delivery if the thread in
 * which it arrives has not called sigsafe_install_tsd.
 * @param signum  The signal number
 * @param handler An optional signal handler which will be run asynchronously.
 *                It will be passed the normal <tt>sigaction(2)</tt>-style
 *                signal information and the <tt>intptr_t</tt> supplied to
 *                sigsafe_install_tsd. The usual async signal-safety rules
 *                apply; it is strongly suggested that this handler do nothing
 *                more than copy whatever data from the <tt>siginfo_t*</tt>
 *                structure to the user-supplied location. This is allowed
 *                since <tt>sigsafe</tt> itself only notes that a signal has
 *                arrived, not even the signal number.
 * @return 0 on success; <tt>-EINVAL</tt> where <tt>sigaction(2)</tt> would
 *         return -1 and set errno to <tt>EINVAL</tt>.
 * @note
 * Call this function at most once for each signal number.
 * @ingroup sigsafe_control
 */
int sigsafe_install_handler(int signum, sigsafe_user_handler_t handler);

/**
 * Installs thread-specific data.
 * Before this is called for a given thread, "safe" signals delivered to that
 * thread will be silently ignored. If you are concerned about signals
 * delivered at thread startup, ensure threads start with blocked signals.
 * @pre This function has not previously been called in this thread.
 * @param userdata   Thread-specific user data which will be delivered to your
 *                   handler routine with every signal.
 * @param destructor An optional destructor for userdata, to be run at thread
 *                   exit.
 * @ingroup sigsafe_control
 */
int sigsafe_install_tsd(intptr_t userdata, void (*destructor)(intptr_t));

/**
 * Clears the signal received flag for this thread.
 * After calling this function, sigsafe system calls will not receive
 * <tt>-EINTR</tt> due to signals received before this call.
 * @pre sigsafe_install_tsd has been called in this thread.
 * @returns The user-specified data given when the TSD was installed for this
 *          thread.
 * @note Additional signals could arrive between a sigsafe sytem call
 * returning <tt>-EINTR</tt> and the heart of this function; it will clear
 * them all. If this is a concern for your application, use the userdata to
 * track signals and check it <i>after</i> calling this function.
 * @note Signals can also be received while you are reading the userdata. This
 * can cause the usual problems like word tearing and stale data. If this is a
 * concern, one approach would be to block signals with
 * <tt>pthread_sigmask(2)</tt> while handling previous ones. (Though remember
 * that some signal delivery mechanisms --- like child process events and
 * interval timers --- simply do not deliver signals if all eligible threads
 * have them masked.)
 * @ingroup sigsafe_control
 */
intptr_t sigsafe_clear_received(void);

/**
 * @defgroup sigsafe_syscalls Signal-safe system call wrappers
 * These are alternate system call wrappers which are guaranteed to return
 * an <tt>EINTR</tt> immediately if a "safe" signal is delivered on or before
 * the transition back to userspace. In particular, they will return
 * <tt>EINTR</tt> if a signal has been delivered before the function call or
 * immediately before the transition to kernel space within the function. They
 * will also return <tt>EINTR</tt> on receipt of a "safe" system call in
 * kernel space even when using <tt>SA_RESTART</tt>. And they return error
 * values as negative numbers rather than through <tt>errno</tt>. These are
 * their sole visible differences from the standardized system calls of the
 * same names. Like the standardized system call wrappers, it will not return
 * with <tt>EINTR</tt> if the system call has already completed.
 * @par Usage example:
 * @code
 * ssize_t retval;
 * while ((retval = sigsafe_read(fd, buf, count)) == -EINTR) {
 *     handle_signal();
 * }
 * if (retval < 0) {
 *     printf("read error %zd (%s)\n", -retval, strerror(-retval));
 * } else if (retval == 0) {
 *     printf("stream end\n");
 * } else {
 *     printf("read %zd bytes\n", retval);
 * }
 * @endcode
 * @par Implementation:
 * Internally, these check a thread-specific value noting if a signal has
 * arrived. They then proceed with no regard to signals. The addresses of key
 * points of the code are known to the signal handler, which allows it to make
 * a long jump to the appropriate branch. Thus, they have virtually no
 * overhead over the standard system call wrappers.
 * @note
 * If you do not see the system call you want here, don't panic. It's very
 * easy to add new system calls in most cases.
 */

/**
 * Signal-safe <tt>read(2)</tt>.
 * @ingroup sigsafe_syscalls
 */
int sigsafe_read(int fd, void *buf, size_t count);

/**
 * Signal-safe <tt>readv(2)</tt>.
 * @ingroup sigsafe_syscalls
 */
int sigsafe_readv(int d, const struct iovec *iov, int iovcnt);

/**
 * Signal-safe <tt>write(2)</tt>.
 * @ingroup sigsafe_syscalls
 */
int sigsafe_write(int fd, const void *buf, size_t count);

/**
 * Signal-safe <tt>writev(2)</tt>.
 * @ingroup sigsafe_syscalls
 */
int sigsafe_writev(int d, const struct iovec *iov, int iovcnt);

/**
 * Signal-safe <tt>epoll_wait(2)</tt>.
 * @par Availability:
 * Linux 2.6+ systems
 * @ingroup sigsafe_syscalls
 */
#if defined(HAVE_EPOLL) || defined(DOXYGEN)
int sigsafe_epoll_wait(int epfd, struct epoll_event *events, int maxevents,
                       int timeout);
#endif

/**
 * Signal-safe <tt>kevent(2)</tt>.
 * @ingroup sigsafe_syscalls
 * Note that the system call itself has a signal handling mechanism. If
 * receiving signals only with <tt>kevent()</tt> is good enough, you do not
 * need to use <tt>sigsafe</tt> to handle signals safely.
 * @par Availability:
 * Modern FreeBSD, NetBSD, OpenBSD, Darwin 7+ (OS X 10.3 Panther)
 */
#if defined(HAVE_KEVENT) || defined(DOXYGEN)
int sigsafe_kevent(int kq, int nchanges, struct kevent **changelist,
                   int nevents, struct kevent **eventlist,
                   struct timespec *timeout);
#endif

/**
 * Signal-safe <tt>select(2)</tt>.
 * @ingroup sigsafe_syscalls
 */
int sigsafe_select(int nfds, fd_set *readfds, fd_set *writefds,
                   fd_set *errorfds, struct timeval *timeout);

/**
 * Signal-safe <tt>poll(2)</tt>.
 * @ingroup sigsafe_syscalls
 */
#if defined(HAVE_POLL) || defined(DOXYGEN)
int sigsafe_poll(struct pollfd *ufds, unsigned int nfds, int timeout);
#endif

/**
 * Signal-safe <tt>wait4(2)</tt>.
 * @ingroup sigsafe_syscalls
 */
int sigsafe_wait4(pid_t wpid, int *status, int options, struct rusage *rusage);

/**
 * Signal-safe <tt>accept(2)</tt>.
 * @ingroup sigsafe_syscalls
 */
int sigsafe_accept(int fd, struct sockaddr *addr, socklen_t *addrlen);

/**
 * Signal-safe <tt>connect(2)</tt>.
 * @ingroup sigsafe_syscalls
 */
int sigsafe_connect(int sockfd, const struct sockaddr *serv_addr,
                    socklen_t addrlen);

/**
 * Signal-safe <tt>nanosleep(2)</tt>.
 * @ingroup sigsafe_syscalls
 */
int sigsafe_nanosleep(const struct timespec *rqtp, struct timespec *rmtp);

#ifdef ORG_SLAMB_SIGSAFE_INTERNAL

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

struct sigsafe_tsd {
    volatile sig_atomic_t signal_received;
    intptr_t user_data;
    void (*destructor)(intptr_t);
};

struct sigsafe_syscall {
    const char *name;
    void *address;
    void *minjmp;
    void *maxjmp;
    void *jmpto;
};

extern struct sigsafe_syscall sigsafe_syscalls[];

extern pthread_key_t sigsafe_key;
extern sigsafe_user_handler_t user_handlers[SIGSAFE_SIGMAX];

void sighandler_for_platform(ucontext_t *ctx);

/* socketcall is on Linux; it can't hurt elsewhere to declare it here */
int sigsafe_socketcall(int call, unsigned long *args);
#endif // ORG_SLAMB_SIGSAFE_INTERNAL

#ifdef __cplusplus
} // extern "C"
#endif
#endif /* !ORG_SLAMB_SIGSAFE_H */
