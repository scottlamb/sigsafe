/** @file
 * Alternate system call wrappers which provide signal safety.
 * See the module description for more in-depth discussion.
 * @legal
 * Copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * This file is part of sigsafe, which is released under the MIT license.
 * @version     $Id$
 * @author      Scott Lamb &lt;slamb@slamb.org&gt;
 */

/**
 * @mainpage sigsafe library for safe signal handling.
 * sigsafe is a C library for safely, reliably, and promptly handling signals
 * delivered to specific threads without significant overhead. This is a
 * common and challenging problem, and sigsafe can help. Please see below for
 * examples.
 *
 * sigsafe includes code, documentation, a performance benchmark, and a
 * correctness tester that exhaustively searches for race conditions with
 * the <tt>ptrace(2)</tt> facility.
 *
 * sigsafe is available for Linux/i386 and Darwin/ppc (OS X). More platforms
 * should be ready soon.
 *
 * Please look at the <tt>README</tt> file for installation notes and porting
 * hints.
 *
 * <h2>Background</h2>
 *
 * (This section is intended to describe what signals are, how they are
 * useful, and the evolution of mechanisms to handle them safely. If you're
 * feeling ambitious, you might skip below to the "Signal-handling patterns"
 * section.)
 *
 * <h3>Uses of signals</h3>
 *
 * On UNIX systems, signals are a common way to receive various types of
 * events, such as:
 *
 * <ul>
 * <li>special key presses for terminal programs (interrupt, suspend,
 *     resume)</li>
 * <li>hangups (closing of a terminal or loss of a connection) for
 *     terminal programs</li>
 * <li>configuration file changes for daemons</li>
 * <li>timeouts, as with <tt>alarm(2)</tt> or <tt>setitimer(2)</tt></li>
 * <li>child process events (started, stopped, resumed, etc.)</li>
 * <li>DNS request completion with the <tt>getaddrinfo_a(3)</tt> API.</li>
 * <li>AIO (asynchronous I/O) completion</li>
 * <li>termination (graceful or otherwise)</li>
 * <li>filesystem change notification under Linux with the <tt>F_NOTIFY</tt>
 *     API.</li>
 * <li>priority input on a socket.</li>
 * </ul>
 *
 * <h3>Handling signals, first approach</h3>
 *
 * When a signal arrives, a function is immediately executed and then control
 * returns to the previously executing function. Signal handlers can be
 * installed like this:
 *
 * @code
 * #include <signal.h>
 *
 * ...
 *
 * void sighandler(int signum) {
 *     printf("Received signal %d\n", signum);
 * }
 *
 * ...
 *
 * int main(void) {
 *     signal(SIGINT, &sighandler);
 *     signal(SIGUSR1, &sighandler);
 *     ...
 *  }
 *  @endcode
 *
 * When a signal arrives, the program will print "Received signal X" and
 * continue as before.
 *
 * ...unless you're unlucky. At many points in a program, it's expected that
 * data structures don't spontaneously change, as they might in a signal
 * handler. The <tt>printf(3)</tt> call in that signal handler is such a case.
 * Internally, it calls <tt>malloc(3)</tt> to allocate memory.
 * <tt>malloc(3)</tt> is not re-entrant, meaning that it is not safe to start
 * execution of one instance while another is running, as can happen inside a
 * signal handler. If this happens, the heap can become corrupted and the
 * program will crash.
 *
 * This is a similar problem to thread safety but arguably much worse.
 * Threads can obtain locks and wait for other threads to complete critical
 * sections. But signals occur at any time and complete before the program
 * resumes normal execution. They can't wait for the main section to complete
 * a critical section; they need to do their work immediately.
 *
 * For this reason, a lot of projects use the approach in the next section.
 *
 * <h3>Setting a flag</h3>
 *
 * We can avoid calling functions unsafely by just setting a flag in our
 * signal handler and then returning control to the main program. The code
 * looks something like this:
 *
 * @code
 * int terminate_signal_received;
 *
 * void sigtermhandler(int signum) {
 *     terminate_signal_received = 1;
 * }
 *
 * ...
 *
 * int main(void) {
 *     ... setup work ...
 *     while (!terminate_signal_received) {
 *         ... handle events ...
 *     }
 *     ... cleanup work ...
 * }
 * @endcode
 *
 * This code is safer, but there are still problems.
 *
 * First, the compiler can optimize the comparison of
 * <tt>terminate_signal_received</tt> into a register. This makes the program
 * faster, but it also breaks the signal handling. The main program never sees
 * the change that the signal handler makes to the value in memory. And the
 * signal handler certainly doesn't know enough to change the value in the
 * register.
 *
 * The solution to this problem is easy: use the <tt>volatile</tt> keyword.
 * This tells the compiler that the value of
 * <tt>terminate_signal_received</tt> can change at any time. Then it always
 * retrieves it from memory immediately before doing a comparison.
 *
 * In general, you should use <tt>volatile sig_atomic_t</tt> values if you are
 * retrieving or modifying them in signal handlers. The <tt>sig_atomic_t</tt>
 * type is designed to prevent a more subtle problem called word tearing,
 * which I will describe later.
 *
 * Okay, that's easy enough. But now there's another problem: we only check
 * the <tt>signal_received</tt> value at each iteration of the loop. What if
 * we're waiting for an IO event inside the loop? In the shutdown sequence for
 * many platforms, a daemon only gets 15 or so seconds to gracefully clean up
 * before it is abruptly terminated. So if the cleanup work is important, the
 * signal must cause the loop iteration to end quickly.
 *
 * Maybe this will help: system calls that wait (block) for events return
 * with <tt>EINTR</tt> immediately if a signal arrives during their operation.
 * (To be precise: we can choose if they do so or not when we install the
 * signal handler.) So we can have code like this:
 *
 * @code
 * while (!signal_received) {
 *     retval = read(fd, buf, count));
 *     if (retval >= 0) {
 *         ... handle IO ...
 *     } else if (errno != EINTR) {
 *         ... error ...
 *     }
 * }
 * @endcode
 *
 * Now if a signal arrives during the read, we proceed to the cleanup code
 * immediately. Good.
 *
 * But...what if a signal arrives between the
 * <tt>!terminate_signal_received</tt> test and the <tt>read(2)</tt> system
 * call? It looks like there's no code there, but there actually is a fair
 * amount. System call functions aren't magic; they are normal functions that
 * somewhere in the middle execute an instruction that passes control to
 * kernel space. There are always going to be instructions between our test of
 * <tt>signal_received</tt> and the system call really starting.
 * Unfortunately, the system call does not return <tt>EINTR</tt> in this case.
 * It doesn't know anything about our little boolean, much less whether or not
 * we've checked it since the last time we received a signal.
 *
 * So <tt>EINTR</tt> does not help us --- there's a race condition. (A window
 * of time in which our program will do the wrong thing.) And it's not as rare
 * as it seems - maybe it is in this example, but for some programs that
 * receive signals very often, it's inevitable that a failure will happen, and
 * soon. To solve this race condition, many people have tried the intricate
 * code patterns below. They all have their downsides, which I will describe.
 * This is also the goal <tt>sigsafe</tt> hopes to accomplish.
 *
 * <h3>Jumping out of the signal handler</h3>
 *
 * Instead of relying on the system call returning, let's try modifying the
 * main program's flow of execution. The C library provides the
 * <tt>sigsetjmp(2)</tt> and <tt>siglongjmp(2)</tt> functions for this
 * purpose. <tt>sigsetjmp(2)</tt> sets up a jump buffer, and
 * <tt>siglongjmp(2)</tt> returns to it. So we can do something like this:
 *
 * @code
 * volatile sig_atomic_t terminate_signal_received;
 * volatile sig_atomic_t jump_is_safe;
 * sigjmp_buf env;
 *
 * void sigtermhandler(int signum) {
 *     terminate_signal_received = 1;
 *     if (jump_is_safe) {
 *         siglongjmp(env, 1);
 *     }
 * }
 *
 * ...
 *
 * while (1) {
 *     sigsetjmp(env, 1);
 *     jump_is_safe = 1;
 *     if (terminate_signal_received) {
 *         jump_is_safe = 0;
 *         break;
 *     }
 *     retval = read(fd, buf, count);
 *     jump_is_safe = 0;
 *     ...
 * }
 * @endcode
 *
 * Now we're getting into tricky code. Through a lot of care, we've avoided
 * several races in the code above:
 *
 * <ul>
 * <li>we always check <tt>terminate_signal_received</tt> <i>after</i> setting
 *     <tt>jump_is_safe</tt> to avoid a race of the same style as before.</li>
 * <li>we always set <tt>env</tt> immediately <i>before</i> setting
 *     <tt>jump_is_safe</tt> to avoid a race that could cause an undefined jump.</li>
 * <li>we never call any async signal-unsafe functions when
 *     <tt>jump_is_safe</tt> is set.</li>
 * <li>we are extremely careful to make sure <tt>jump_is_safe</tt> is set to 0
 *     when leaving this block of code. There are three ways:
 *     <ol>
 *     <li>the "normal" path where no signal arrives</li>
 *     <li>jumping from the signal handler (and of course seeing that
 *         <tt>terminate_signal_received</tt> is true)</li>
 *     <li>seeing <tt>terminate_signal_received</tt> became true before we set
 *         <tt>jump_is_safe</tt>.</li>
 *     </ol>
 *     Paths 2 and 3 may seem exactly the same, but we could easily have
 *     broken 3 by relying on the signal handler to set <tt>jump_is_safe</tt>
 *     to 0 before exiting.</li>
 * </ul>
 *
 * But with all that work, there's <i>still</i> a race condition. If our
 * system call completes, there's some time after when an arriving signal
 * would cause us to take the signal received path. We've received data, but
 * we don't have any way of knowing that. That's no good - most protocols
 * don't have a way of asking "did you just say something?" so we need to
 * reliably handle every read. It could have been something important that we
 * need to record before shutting down.
 *
 * So what can we do? We could set <tt>retval</tt> to some never-returned
 * value before like -2. Then if it changes, we know the system call has
 * returned. But that's not reliable. On most platforms, the return value from
 * a system call is stored in a register. So there's always at least one
 * instruction in which a signal could arrive without changing
 * <tt>retval</tt>, and probably many more. <tt>errno</tt> is even worse,
 * because libc goes through more indirection to store it in thread-specific
 * data. (Something we'll talk about later when we deal with threading.) So
 * that doesn't help at all.
 *
 * (Watch this space. There are several other solutions to the problem that I
 * mention below. I intend to describe them in more detail here.)
 *
 * <h2>Signal handling patterns without sigsafe</h2>
 * This library is designed to replace the following problematic patterns:
 * <ol>
 * <li>Calling async signal-unsafe functions from signal handlers.
 * @code
 * void unsafe_sighandler_a(int signum) {
 *     printf("Received signal %d\n", signum);
 * }
 * @endcode
 *
 * @code
 * void unsafe_sighandler_b(int signum) {
 *     mylist->tail = (struct mylist*) malloc(sizeof(mylist));
 *     ...
 * }
 * @endcode
 * SUSv3 (the Single UNIX Specification, version 3) defines a list of
 * functions which are safe from any time from signal handlers. It's a very
 * short list. In particular, you must not call <tt>malloc(3)</tt> from a
 * signal handler, or any function which depends on it. Failures are rare
 * enough that people think their code is correct, but this can lead to subtle
 * bugs.</li>
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
 *     if (jump_is_safe)
 *         siglongjmp(env, 1);
 * }
 *
 * ...
 *
 * sigsetjmp(signal_received, 1);
 * jump_is_safe = 1;
 * if (!signal_received) {
 *     retval = syscall();
 * }
 * jump_is_safe = 0;
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
 * state. (This requirement is shared by <tt>sigsafe</tt>.)
 * <p>It's also very hard to implement correctly. Notice several things about
 * the code fragment above:</p>
 * <ul>
 * <li>it ensures <tt>jump_is_safe</tt> is set only after
 *     <tt>sigsetjmp(2)</tt> is called and only for a very narrow window in
 *     which no async signal-unsafe functions are called.</li>
 * <li>it uses <tt>sigsetjmp(2)</tt> and <tt>siglongjmp(2)</tt> rather than
 *     <tt>setjmp(2)</tt> and <tt>longjmp(2)</tt>; you can specify the signal
 *     mask restoration behavior of the sig variants, while it is defined by
 *     the platform for the plain functions.</li>
 * <li>it checks <tt>signal_received</tt> <i>after</i> setting
 *     <tt>jump_is_safe</tt>.</li>
 * </ul>
 * <p>These are all important!</p>
 * <p>Also there's a performance problem - <tt>sigsetjmp(..., 1)</tt> makes a
 * system call to retrieve the signal mask, so you're slowing down every
 * iteration for correct signal behavior. To avoid that, you'd have to think
 * about the signal mask yourself. Even more opportunities for bugs.</p>
 * <li>Using <tt>pselect(2)</tt>. This function is supposed to change the
 * signal mask atomically in the kernel for the duration of operation,
 * supporting error-free operation like this:
 * @code
 * sigset_t blocked, unblocked;
 * int retval;
 *
 * pthread_sigmask(SIG_SETMASK, &blocked, NULL);
 *
 * ...
 *
 * while ((retval = pselect(..., &unblocked)) == -1 && errno == EINTR) {
 *     printf("Signal received.\n");
 * }
 *
 * ...
 * @endcode
 * However, some implementations (notably Linux!) are wrong --- they simply
 * surround a <tt>select(2)</tt> call with <tt>pthread_sigmask(2)</tt>
 * calls. Thus, <tt>pselect(2)</tt> may not return <tt>EINTR</tt> when you
 * expect it to.</li>
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
 * <li>Thread cancellation. In theory, thread cancellation allows for correct
 * operation. In practice, no libc has an acceptable implementation. See my
 * cancellation tests for details, but it will be a long time before this is
 * an acceptable option.</li>
 * <li>...and many other schemes.</li>
 * </ol>
 * <h2>The solution</h2>
 * With <tt>sigsafe</tt>, you can write code like this:
 * @code
 *
 * void myhandler(int signum, siginfo_t *info, void *ctx, intptr_t user_data) {
 *     sigaddset((sigset_t*) user_data, signum);
 * }
 *
 * int main(void) {
 *     ...
 *     sigsafe_install_handler(SIGUSR1, &myhandler);
 *     sigsafe_install_handler(SIGUSR2, &myhandler);
 *     ...
 *  }
 *
 *  ...
 *
 * void* thread_entry(void *arg) {
 *     ...
 *     sigsafe_install_tsd((intptr_t) malloc(sizeof sigset_t), &free);
 *     ...
 * }
 *
 * void read_some_data(void) {
 *     int retval;
 *
 *     while ((retval = sigsafe_read(fd, buf, count)) == -EINTR) {
 *         sigset_t *received = (sigset_t*) sigsafe_clear_received();
 *         if (sigismember(received, SIGUSR1)) {
 *             printf("Received USR1 signal\n");
 *         }
 *         if (sigismember(received, SIGUSR2)) {
 *             printf("Received USR2 signal\n");
 *         }
 *     }
 *     ...
 * }
 * @endcode
 *
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
 * The <tt>sigsafe</tt> library is non-portable! Everything here relies on
 * alternate system call wrappers implemented in assembly and a signal handler
 * which adjusts the instruction pointer when signals arrive in system calls.
 * This means that there is significant work involved in porting it to a new
 * platform (where platform is a combination of OS and architecture).
 *
 * Additionally, it makes the same assumption as all other methods for
 * handling thread-directed signals (with the exception of <tt>kevent(2)</tt>
 * handling), that <tt>pthread_getspecific(2)</tt> is async signal-safe. This
 * is not guaranteed by SUSv3.
 *
 * @legal
 * sigsafe is copyright &copy; 2004 Scott Lamb &lt;slamb@slamb.org&gt;.
 * It is released under the MIT license. See the <tt>README</tt> file for the
 * full license text.
 */

#ifndef ORG_SLAMB_SIGSAFE_H
#define ORG_SLAMB_SIGSAFE_H

#include <signal.h>
#include <ucontext.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/time.h>
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
 * @defgroup sigsafe_syscalls System call wrappers
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

#if defined(NSIG)
#define SIGSAFE_NSIG NSIG
#elif defined(_NSIG)
#define SIGSAFE_NSIG _NSIG
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
extern sigsafe_user_handler_t user_handlers[SIGSAFE_NSIG];

void sighandler_for_platform(ucontext_t *ctx);

/* socketcall is on Linux; it can't hurt elsewhere to declare it here */
int sigsafe_socketcall(int call, unsigned long *args);
#endif // ORG_SLAMB_SIGSAFE_INTERNAL

#ifdef __cplusplus
} // extern "C"
#endif
#endif /* !ORG_SLAMB_SIGSAFE_H */
