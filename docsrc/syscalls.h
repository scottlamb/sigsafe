// $Id$

/**
 * @defgroup syscalls System calls
 * These are the "normal" system calls for the platform, either standardized
 * through the Single UNIX Specification version 3 or specific to the
 * platform. The documentation here describes signal behaviors that I've found
 * to be poorly described in the manual pages. It applies to both the libc
 * system call wrappers and the sigsafe ones (where they exist).
 */
/*@{*/

/**
 * Sets an alarm clock.
 * Generates an asynchronous, process-directed <tt>SIGALRM</tt> signal after
 * the specified number of seconds.
 */
unsigned int alarm(unsigned int seconds);

/**
 * Sets an interval timer.
 * Generates asynchronous, process-directed <tt>SIGALRM</tt>,
 * <tt>SIGVTALRM</tt>, or <tt>SIGPROF</tt> signals.
 */
int setitimer (int, const struct itimerval *new, struct itimerval *old);

/**
 * Sends an asynchronous, thread-directed signal.
 * @note This is despite the note in SUSv3 that asynchronous, thread-directed
 * signals do not exist.
 */
int pthread_kill(pthread_t thread, int signo);

/**
 * Sends an asynchronous, process-directed signal.
 */
int kill(pid_t pid, int signo);

/**
 * Reads from a file descriptor.
 * A synchronous, thread-directed <tt>SIGPIPE</tt> is delivered during this
 * system call if the opposite end of the pipe is closed. I recommend ignoring
 * this signal, which is worthless. It was implemented solely to be a more
 * abrupt error to programs that do not check return values carefully. The 0
 * return says the same thing.
 */
ssize_t read(int fd, void *buf, size_t nbytes);

/**
 * Writes to a file descriptor.
 * A synchronous, thread-directed <tt>SIGPIPE</tt> is delivered during this
 * system call if the opposite end of the pipe is closed. I recommend ignoring
 * this signal, which is worthless. It was implemented solely to be a more
 * abrupt error to programs that do not check return values carefully. The
 * <tt>EPIPE</tt> error says the same thing.
 */
ssize_t write(int fd, const void *buf, size_t nbytes);

/*@}*/
