/**
 * @file sigsafe.h Alternate system call wrappers which provide signal safety.
 * See the module description for more in-depth discussion.
 * <p>This file is released under the MIT license.</p>
 * @version $Id$
 * @author  Scott Lamb &lt;slamb@slamb.org&gt;
 */

/**
 * @mainpage <tt>sigsafe</tt> library for safe signal handling.
 * A project I'm toying with, to implement a truly efficient, safe, reliable
 * method of delivering signals to specific threads.
 *
 * Thread cancellation really accomplishes 99% of what I want to do here, but
 * unfortunately thread cancellation does not work. (See the cancellation_tests
 * directory for justification of that statement.)
 *
 * To really do this well, I would need to write assembly code for each
 * combination of operating system and processor on which I want to run my code.
 * So far I have done so only for Darwin/ppc.
 *
 * See details on how this works in the "Modules" section.
 */

#ifndef ORG_SLAMB_SIGSAFE_H
#define ORG_SLAMB_SIGSAFE_H

#include <signal.h>
#include <ucontext.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifdef HAVE_EPOLL
#include <sys/epoll.h>
#endif
#ifdef HAVE_POLL
#include <sys/poll.h>
#endif
#include <unistd.h>
#include <stddef.h>
#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup sigsafe Safe signals
 * These functions provide a safe way for handling signals in (potentially
 * multi-threaded) programs. In particular, they are designed to replace the
 * following problematic patterns:
 * <ol>
 * <li>Polling for a variable before system calls and on <tt>EINTR</tt>:
 * @code
 * volatile sig_atomic_t signal_received;
 *
 * void sighandler(int) { signal_received++; }
 *
 * ...
 *
 * int retval;
 * do {
 *     if (signal_received) { handle_signal(); }
 * } while ((retval = syscall()) == -1 && errno == EINTR) ;
 * @endcode
 * In this pattern, there is a race condition between the check for
 * <tt>signal_received</tt> and <tt>syscall()</tt> actually entering kernel
 * space. If a signal arrives in this time, it will not force <tt>EINTR</tt>
 * and the signal delivery could be delayed indefinitely.</li>
 * <li>Using a <tt>sigjmp_buf</tt> to immediately return from system calls.
 * @code
 * volatile sig_atomic_t signal_received, jump_is_safe;
 * sigjmp_buf env;
 *
 * void sighandler(int) {
 *     signal_received++;
 *     if (jump_is_safe) siglongjmp(env, 1);
 * }
 *
 * ...
 *
 * if (sigsetjmp(signal_received, 0) || signal_received) {
 *     handle_signal();
 * }
 * int retval;
 * while ((retval = syscall()) == -1 && errno == EINTR) ;
 * @endcode
 * This has a different race condition: if a signal arrives, it is impossible
 * to tell if the system call completed and, if so, what its result was. This
 * affects different calls differently:
 * <ul>
 *   <li><tt>select(2)</tt>, <tt>poll(2)</tt>, or level-triggered
 *       <tt>epoll_wait(2)</tt>/<tt>kevent(2)</tt>: No problem; the call can
 *       be safely repeated.</li>
 *   <li>Edge-triggered <tt>epoll_wait(2)</tt>/<tt>kevent(2)</tt>: It is
 *       impossible to know now what descriptors have data available, since
 *       subsequent calls will no longer return these descriptors. A
 *       level-triggered poll mechanism would have to be used in this case,
 *       which complicates the code greatly.</li>
 *   <li><tt>read(2)</tt>, <tt>readv(2)</tt>, <tt>write(2)</tt>, or
 *       <tt>writev(2)</tt>: It is impossible to know if the IO operation
 *       completed successfully.</li>
 * </ul>
 * This also relies on jumping from a signal handler to be safe; this is not
 * defined by SUSv3 and notably is false on Cygwin. Linux and Solaris do
 * support this behavior, though neither correctly restores the cancellation
 * state.
 * <li>Replacing blocking IO calls with <tt>poll(2)</tt> calls and
 * non-blocking IO calls:
 * @code
 * int signal_pipe[2];
 *
 * void sighandler(int signo) { write(signal_pipe[1], &signo, sizeof(int)); }
 *
 * ...
 *
 * struct pollfd fds[2] = {
 *     {fd,             POLLIN, 0},
 *     {signal_pipe[0], POLLIN, 0}
 * };
 *
 * retval = poll(fds, 2, 0);
 * ...
 * if (fds[1] & POLLIN) { ...; handle_signals(); }
 * ...
 * retval = read(fd, buf, count);
 * @endcode
 * This method is correct but slow, since it doubles the number of system
 * calls to be made on basic IO operations.</li>
 * </ol>
 * @note
 * This is not the One True Method for correct signal handling. In particular,
 * there are two other methods you should consider:
 * <ol>
 * <li>handling all signals in a single thread. If you do not use
 *     thread-directed signals for internal signaling (timeouts, etc.),
 *     blocking signals everywhere and using <tt>sigwaitinfo(2)</tt> may be
 *     your easiest correct way.</li>
 * <li>Handling signals with polling functions. If you exclusively use
 *     non-blocking IO, <tt>kevent(2)</tt>'s built-in signal mechanism or the
 *     pipe-write-from-signal-handler methods may work well for you.</li>
 * </ol>
 * @warning
 * The <tt>sigsafe</tt> library is extremely non-portable! Everything here
 * relies on alternate system call wrappers being implemented in C. This means
 * that there is <i>significant</i> work involved in porting it to a new
 * platform (where platform is a combination of OS and architecture).
 *
 * Additionally, it makes the same assumption as all other methods for
 * handling thread-directed signals (with the exception of <tt>kevent(2)</tt>
 * handling), that <tt>pthread_getspecific(2)</tt> is async signal-safe. This
 * is not guaranteed by SUSv3.
 */

/**
 * @defgroup sigsafe_control Signal control functions
 * @ingroup sigsafe
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
 * If you install any signal handlers through other methods, you should ensure
 * that they mask the "safe" signals, as jumping from nested signal handlers
 * is not safe.
 * @note
 * Call this function at most once for each signal number.
 * @ingroup sigsafe_control
 */
int sigsafe_install_handler(int signum, sigsafe_user_handler_t);

/**
 * Installs thread-specific data.
 * Before this is called for a given thread, "safe" signals delivered to that
 * thread will be silently ignored. If you are concerned about signals
 * delivered at thread startup, ensure threads start with blocked signals.
 * @param userdata   Thread-specific user data which will be delivered to your
 *                   handler routine with every signal.
 * @param destructor An optional destructor for userdata, to be run at thread
 *                   exit.
 * @ingroup sigsafe_control
 */
int sigsafe_install_tsd(intptr_t userdata, void (*destructor)(intptr_t));

/**
 * @defgroup sigsafe_syscalls System call wrappers
 * @ingroup sigsafe
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
int sigsafe_kevent(int kq, int nchanges, struct kevent **changelist,
                   int nevents, struct kevent **eventlist,
                   struct timespec *timeout);

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

#ifdef ORG_SLAMB_SIGSAFE_INTERNAL
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
extern sigsafe_user_handler_t user_handlers[NSIG];

void sighandler_for_platform(ucontext_t *ctx);
#endif // ORG_SLAMB_SIGSAFE_INTERNAL

#ifdef __cplusplus
} // extern "C"
#endif
#endif /* !ORG_SLAMB_SIGSAFE_H */
