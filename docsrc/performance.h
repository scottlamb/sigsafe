// $Id$

/**
 * @page performance Performance
 * In general, performance should not be your main reason for using sigsafe.
 * However, here I will show that performance is at least comparable to libc's
 * system call wrappers, unsafe signals and all. It may be significantly
 * faster than some ways of handling signals safely, notably the self-pipe
 * trick. It certainly is faster, but the jury is still out on whether this is
 * a significant difference.
 *
 * This <i>will</i> contain pretty graphs of benchmarks of sigsafe benchmarks
 * (a microbenchmark and a use in a real application). But I haven't made them
 * yet. Until then, you're welcome to run the microbenchmark yourself. Try
 * comparing the output from:
 *
 * - <tt>time build-myplatform/tests/bench_read_raw</tt> - this tests the
 *   libc's system call wrappers in a plain way without safe signal handling.
 * - <tt>time build-myplatform/tests/bench_read_safe</tt> - this is sigsafe's
 *   handling. In theory, it should be very slightly slower than the libc's.
 *   In practice, it is actually slightly faster in some cases! (This implies
 *   a suboptimal libc.) Let me know if it is significantly slower.  (It may
 *   be so on the Pentium IV, because I have yet to implement
 *   <tt>SYSENTER</tt> support.)
 * - <tt>time build-platform/tests/bench_read_select</tt> - this is a test
 *   with every read preceded by a <tt>select</tt>, as is necessary in some
 *   cases with the self-pipe trick commonly used as an alternative to
 *   sigsafe. It should be about half the speed.
 *
 * The real-world benchmark will likely be Apache. I've made a patch that
 * eliminates a need to use <tt>select</tt> before <tt>read</tt> and
 * <tt>write</tt> for socket timeouts. There are actually no signals involved,
 * but it's a very analogous situation. I expect to find that under platforms
 * with high system call overhead (notably Darwin), it is noticeably faster.
 * Unfortunately, I've run into hardware problems while benchmarking. Stay
 * tuned...
 */
