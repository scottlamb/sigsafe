/** @file
 * Alternate system call wrappers which provide signal safety.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

#ifndef SIGSAFE_H
#define SIGSAFE_H

#ifdef __NetBSD__
#define SIGSAFE_NO_SIGINFO
#endif

#include <sigsafe_config.h>
#include <signal.h>
#ifndef SIGSAFE_NO_SIGINFO
#include <ucontext.h>
#endif
#include <sys/select.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>
#ifdef SIGSAFE_HAVE_STDINT_H
#include <stdint.h> /* for intptr_t */
#endif

#ifdef SIGSAFE_HAVE_EPOLL
#include <sys/epoll.h>
#endif

#ifdef SIGSAFE_HAVE_POLL
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
/*@{*/

/**
 * User-specified handler type.
 * Arguments:
 * - <tt>int signo</tt>: The signal number received.
 * - <tt>siginfo_t *si</tt>: The signal information as passed to a
 *   sigaction-style signal handler.
 * - <tt>ucontext_t *ctx</tt>: The machine context of the program when
 *   the signal was received. After the user-defined handler exits, the
 *   platform-specific handler will kick in. It will decide if it is currently
 *   executing in a "jump region" of a sigsafe system call and adjust the
 *   instruction pointer if so. While <tt>ctx</tt> is not marked
 *   <tt>const</tt>, you should be very cautious modifying it. Your code will
 *   be non-portable, and you may interfere with sigsafe's operation. In fact,
 *   normally you will not need to even read this parameter.
 * - intptr_t user_data. The data you passed to sigsafe_install_tsd() in this
 *   thread.
 *
 * @warning
 * This handler is executed asynchronously. You must take care to only call
 * async signal-safe functions. In fact, the entire point of sigsafe is to
 * allow you to do very little here and handle the rest in the main program.
 * It's recommended that you only note details about the signal here, not take
 * any action.
 *
 * @par Portability:
 * Unfortunately, NetBSD does not support sigaction, as of the latest released
 * version (1.6.2). Thus, it has a different signature:
 * - <tt>int signo</tt>
 * - <tt>int code</tt> - similar to si->si_code with the standard handler
 * - <tt>struct sigcontext *ctx</tt> - similar to the standard handler's ctx.
 * - <tt>intptr_t user_data</tt>
 * Currently, you will need to use <tt>#ifdef SIGSAFE_NO_SIGINFO</tt> if you
 * wish to support NetBSD.
 *
 * @par Future releases:
 * I may change the API to have "simple" and "extended" versions of user
 * handlers, so that simple ones do not need to worry about these platform
 * differences.
 * @see sigsafe_install_handler
 */
#ifdef SIGSAFE_NO_SIGINFO
typedef void (*sigsafe_user_handler_t)(int, int, struct sigcontext*, intptr_t);
#else
typedef void (*sigsafe_user_handler_t)(int, siginfo_t*, ucontext_t*, intptr_t);
#endif

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
 *                arrived, not even the signal number. May be NULL, in which
 *                case sigsafe simply notes that a signal was received.
 * @return 0 on success; <tt>-EINVAL</tt> where <tt>sigaction(2)</tt> would
 *         return -1 and set errno to <tt>EINVAL</tt>.
 * @note
 * Call this function at most once for each signal number.
 */
int sigsafe_install_handler(int signum, sigsafe_user_handler_t handler);

/**
 * Installs thread-specific data.
 * Before this is called for a given thread, "safe" signals delivered to that
 * thread will be silently ignored. If you are concerned about signals
 * delivered at thread startup, ensure threads start with blocked signals.
 * @note This function still must be called for single-threaded compiles.
 * @pre This function has not previously been called in this thread.
 * @param userdata   Thread-specific user data which will be delivered to your
 *                   handler routine with every signal.
 * @param destructor An optional destructor for userdata, to be run at thread
 *                   exit. It is unspecified whether this runs for the final
 *                   thread to exit - TSD destructors are used to clean up
 *                   memory, and that happens on process exit automatically.
 *                   Some pthread implementations vary. In single-threaded
 *                   compiles, this will be ignored.
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
 */
intptr_t sigsafe_clear_received(void);

/*@}*/

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
/*@{*/

/** Signal-safe <tt>read(2)</tt>. */
int sigsafe_read(int fd, void *buf, size_t count);

/** Signal-safe <tt>readv(2)</tt>. */
int sigsafe_readv(int d, const struct iovec *iov, int iovcnt);

/** Signal-safe <tt>write(2)</tt>. */
int sigsafe_write(int fd, const void *buf, size_t count);

/** Signal-safe <tt>writev(2)</tt>. */
int sigsafe_writev(int d, const struct iovec *iov, int iovcnt);

/**
 * Signal-safe <tt>epoll_wait(2)</tt>.
 * @par Availability:
 * Linux 2.6+ systems, possibly backported to some Linux 2.4 systems.
 * This function will exist if <tt>epoll_wait(2)</tt> was in the system
 * headers when sigsafe was compiled. However, it will return <tt>-ENOSYS</tt>
 * unless it exists in the currently-running kernel.
 */
#if defined(SIGSAFE_HAVE_EPOLL) || defined(DOXYGEN)
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
#if defined(SIGSAFE_HAVE_KEVENT) || defined(DOXYGEN)
int sigsafe_kevent(int kq, int nchanges, struct kevent **changelist,
                   int nevents, struct kevent **eventlist,
                   struct timespec *timeout);
#endif

/**
 * Signal-safe <tt>select(2)</tt>.
 * @par Availability:
 * All supported systems except Solaris.
 * (The <tt>select(2)</tt> system call on Solaris is emulated with
 * <tt>poll(2)</tt>.)
 */
#if defined(SIGSAFE_HAVE_SELECT) || defined(DOXYGEN)
int sigsafe_select(int nfds, fd_set *readfds, fd_set *writefds,
                   fd_set *errorfds, struct timeval *timeout);
#endif

/**
 * Signal-safe <tt>poll(2)</tt>.
 * @par Availability:
 * All supported systems except Darwin/OS X.
 * (The <tt>poll(2)</tt> system call on Darwin is emulated with
 * <tt>select(2)</tt>.)
 */
#if defined(SIGSAFE_HAVE_POLL) || defined(DOXYGEN)
int sigsafe_poll(struct pollfd *ufds, unsigned int nfds, int timeout);
#endif

/** Signal-safe <tt>wait4(2)</tt>. */
int sigsafe_wait4(pid_t wpid, int *status, int options, struct rusage *rusage);

/**
 * Signal-safe <tt>accept(2)</tt>.
 * @ingroup sigsafe_syscalls
 */
int sigsafe_accept(int fd, struct sockaddr *addr, socklen_t *addrlen);

/** Signal-safe <tt>connect(2)</tt>. */
int sigsafe_connect(int sockfd, const struct sockaddr *serv_addr,
                    socklen_t addrlen);

/**
 * Signal-safe <tt>nanosleep(2)</tt>.
 * @par Availability:
 * Currently not implemented on Tru64/alpha or Darwin/ppc. I expect it will be
 * in a future release.
 */
int sigsafe_nanosleep(const struct timespec *rqtp, struct timespec *rmtp);

/*@}*/

#ifdef __cplusplus
} // extern "C"
#endif
#endif /* !SIGSAFE_H */
