// $Id$

/**
 * @page backgnd Background information
 *
 * (This section is intended to describe what signals are, how they are
 * useful, and the evolution of mechanisms to handle them safely. If you're
 * feeling ambitious, you might skip to the "Signal-handling patterns"
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
 * Don't be alarmed if you find the above confusing. It is hard code to write
 * correctly, which is one major disadvantage of this approach. Through a lot
 * of care, we've avoided several races in the code above:
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
 * (Aside: there's one more disadvantage - portability. This approach is
 * <i>not</i> guaranteed by the Single UNIX Specification, or any other
 * standard I'm aware of. Some platforms just do not support jumping out of
 * signal handlers. I'm aware of problems with Cygwin and user threading
 * libraries. You might well find this acceptable - sigsafe does not currently
 * run on any such platforms, and I have no immediate plans to try.)
 *
 * <h3>The self-pipe trick</h3>
 *
 * There <i>is</i> a portable, reliable way of doing this with, but only for
 * some system calls. Let's imagine a slightly different situation: instead of
 * waiting to read from a descriptor (or a signal), we're waiting for
 * availability of several descriptors. (We can use select(), kevent(),
 * epoll_wait(), poll(), etc.)
 *
 * Then we can turn a signal into another file descriptor like this:
 *
 * @code
 * enum PipeHalf { READ = 0, WRITE = 1 };
 * int signalPipe[2];
 *
 * void pipehandler(int signo) { char c; write(signalPipe[WRITE], &c, 1); }
 *
 * ...
 *
 * struct sigaction sa;
 * pipe(signalPipe);
 * sa.sa_handler = &pipehandler;
 * sa.sa_flags = SA_RESTART;
 * sigfillset(&sa.sa_mask);
 * sigaction(SIGUSR1, &sa, NULL);
 * @endcode
 *
 * We can then just add <tt>signalPipe[READ]</tt> to the list of descriptors
 * to wait for. When we read data from it, we know that a signal has arrived.
 * Much, much less error-prone than the approach above.
 *
 * The catch is that it only works when waiting for IO availability.
 *
 * We can change the read() code above to do fairly easily, but there's a
 * performance penalty: now whenever we want to do a read, we have to do two
 * systems calls (the select() and the read()). On some platforms (OS X), the
 * high system call latency can make this a problem. ([TODO: add concrete
 * numbers. Like ones from that Apache timeout code.])
 *
 * For other system calls, there's no clear way to do the same thing. Consider
 * waiting for a child to exit or for a keyboard interrupt, as the Bourne
 * shell does. We could wait for one of two signals: <tt>SIGCHLD</tt> or
 * <tt>SIGINT</tt>. (Either using the self-pipe trick or by sigsuspend(),
 * which we'll discuss later.) Then, on <tt>SIGCHLD</tt>, we could call
 * waitpid() to learn the details. But that's not the most natural way to do
 * things. It would be easier if we could use all the system calls as they
 * were originally intended, and get correct signal behavior.
 *
 * <h3>sigsafe</h3>
 *
 * sigsafe provides this behavior --- natural use of system calls with a
 * reliable mechanism to tell when a signal arrives. Each sigsafe_XXX()
 * function executes the standardized system call of the same name, but
 * provides a different C wrapper around it than the one in libc. This allows
 * it to provide more useful signal behavior. <tt>EINTR</tt> is returned if a
 * signal is returned during <i>or before</i> the system call. It sets a flag
 * that can be cleared with sigsafe_clear_received(). Thus, you can use the
 * standard functions in the intuitive way without error-prone and
 * performance-impairing tricks. [TODO: hyperlink to module doc]
 *
 * <h3>Blocked signals</h3>
 *
 * ...
 *
 * <h3>Realtime signals</h3>
 *
 * ...
 *
 * <h3>Synchronous signals</h3>
 *
 * ...
 *
 * <h3>Thread-directed vs. process-directed signals</h3>
 *
 * ...
 */
