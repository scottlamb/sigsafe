// $Id$

/**
 * @page patternref Pattern reference
 * Below are a list of signal handling patterns with associated safety,
 * performance, and portability notes.
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
