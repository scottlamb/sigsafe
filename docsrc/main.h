// $Id$

/**
 * @mainpage sigsafe library for safe signal handling.
 * sigsafe is a C library for safely, reliably, and promptly handling signals
 * delivered to specific threads without significant overhead. It includes
 * documentation, a performance benchmark, and a correctness tester that
 * exhaustively searches for race conditions with the <tt>ptrace(2)</tt>
 * facility.
 *
 * The meat of the library is a set of alternate system call wrappers. The
 * shows when signals cause system calls to return immediately:
 *
 * <table>
 *   <tr>
 *     <th align="left">Signal arrival</th>
 *     <th align="left">normal syscall + null handler</th>
 *     <th align="left">normal syscall + flag handler</th>
 *     <th align="left">normal syscall + <tt>longjmp()</tt> handler</th>
 *     <th align="left">sigsafe syscall</th>
 *   </tr>
 *   <tr>
 *     <td>Well before entering kernel</td>
 *     <td style="background-color: #f88">No (signal lost)</td>
 *     <td style="background-color: #8f8">Yes</td>
 *     <td style="background-color: #8f8">Yes</td>
 *     <td style="background-color: #8f8">Yes</td>
 *   </tr>
 *   <tr>
 *     <td>Right before entering kernel</td>
 *     <td style="background-color: #f88">No (signal lost)</td>
 *     <td style="background-color: #f88">No (signal noted)</td>
 *     <td style="background-color: #8f8">Yes</td>
 *     <td style="background-color: #8f8">Yes</td>
 *   </tr>
 *   <tr>
 *     <td>While in kernel</td>
 *     <td style="background-color: #8f8">Yes</td>
 *     <td style="background-color: #8f8">Yes</td>
 *     <td style="background-color: #8f8">Yes</td>
 *     <td style="background-color: #8f8">Yes</td>
 *   </tr>
 *   <tr>
 *     <td>Right after exiting kernel</td>
 *      <td style="background-color: #8f8">No (normal return)</td>
 *      <td style="background-color: #8f8">No (normal return)</td>
 *      <td style="background-color: #f88">Yes (clobbers syscall return)</td>
 *      <td style="background-color: #8f8">No (normal return)</td>
 *   </tr>
 * </table>
 *
 * All sigsafe system calls:
 *
 * - consistently return immediately with <tt>EINTR</tt> if a signal arrives
 *   right before kernel entry.
 * - consistently return immediately with the normal result if a signal
 *   arrives right after kernel exit.
 *
 * It is not possible to create these guarantees with the standard system call
 * wrappers. And they are extremely useful guarantees - you can handle many
 * signals safely without them, but often with a performance penalty or with
 * great difficulty.
 *
 * <h3>Performance</h3>
 *
 * sigsafe's goal is to allow every system call to have correct behavior when
 * signals arrive, without compromising speed when signals do not arrive. As
 * most system calls should not be interrupted by a signal, this is a
 * necessary and sufficient condition for saying a signal handling method has
 * good performance.
 *
 * Another common correct way of handling signals is to set up a pipe for
 * signal handling and write to it in the signal handler. If you're already
 * polling for multiple IO sources, this works well. However, if you're using
 * blocking IO, you have to change to non-blocking and precede all
 * <tt>read()</tt> and <tt>write()</tt> with a <tt>select()</tt>. Thus, the
 * system call overhead is doubled in the most common case. For this reason,
 * <tt>sigsafe</tt> is often superior to this method.
 *
 * <h3>Implementation</h3>
 *
 * sigsafe is implemented through a set of system call wrappers implemented in
 * assembly for each platform. The system call wrappers retrieve a "signal
 * received" flag from memory and return <tt>EINTR</tt> if it is true shortly
 * before entering the kernel. If a signal is received after this value is
 * retrieved, <tt>sigsafe</tt>'s signal handler manually adjusts the
 * instruction pointer to force <tt>EINTR</tt> return.
 *
 * It sounds like a horrible kludge (and maybe it is), but it works reliably
 * and performs well.  But don't take my word for it - verify it yourself with
 * the included race condition checker and benchmarks.
 *
 * <h3>Additional information</h3>
 *
 * - @link background Background information. @endlink (In progress.)
 *   If everything above was confusing to you, this should help you understand
 *   what signals are, why most code does not handle them safely, and how your
 *   code can.
 * - @link goalref Goal-based reference. @endlink (Still to write.)
 *   For writing new code. (I want to wait for blocking IO
 *   or a timeout, how should I do that?)
 * - @link patternref Pattern-based reference. @endlink (In progress.)
 *   For auditing existing code. (Is this code safe? Does it
 *   perform as well as it could? Is it portable?)
 * - @link porting Porting to new systems. @endlink (In progress.)
 *   Tips for writing the platform-dependent portions for new platforms and
 *   testing the results.
 * - @link performance Performance test results. @endlink (In progress.)
 * - The API reference itself. The "Modules" page is a good place to start.
 *
 * <h3>Availability</h3>
 *
 * sigsafe is available for the following platforms:
 *
 * - Darwin/ppc (a.k.a OS X)
 * - FreeBSD/i386
 * - Linux/alpha
 * - Linux/i386
 * - Linux/ia64
 * - Linux/x86_64
 * - NetBSD/i386
 * - Solaris/sparc
 * - Tru64/alpha
 *
 * I still intend to port it to:
 *
 * - FreeBSD/alpha
 * - HP-UX/ia64
 * - HP-UX/parisc
 * - Linux/parisc
 * - Solaris/i386
 *
 * At which point I will be out of machines to port it to. If you want a
 * platform not listed, you'll have to give me access to such a machine or
 * port it yourself.
 */
